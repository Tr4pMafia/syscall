#include <bfvmm/hve/arch/intel_x64/exit_handler/exit_handler.h>
#include <bfvmm/hve/arch/intel_x64/vcpu/vcpu.h>
#include <bfvmm/vcpu/vcpu.h>
#include <bfdebug.h>
#include <bfvmm/vcpu/vcpu_factory.h>
#include <bfgsl.h>
#include <bfstring.h>
#include <string>

namespace mafia
{
namespace intel_x64
{

static const uint64_t yaju = 0x114514ULL;
static uint64_t original_ia32_lstar[4];

bool
advance4syscall(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs) noexcept
{
    vmcs->save_state()->rip = mafia::intel_x64::original_ia32_lstar[vmcs->save_state()->vcpuid];
    return true;
}

::x64::msrs::value_type
emulate_rdmsr_mafia(::x64::msrs::field_type msr)
{
    switch (msr) {
        case ::intel_x64::msrs::ia32_debugctl::addr:
            return ::intel_x64::vmcs::guest_ia32_debugctl::get();

        case ::x64::msrs::ia32_pat::addr:
            return ::intel_x64::vmcs::guest_ia32_pat::get();

        case ::intel_x64::msrs::ia32_efer::addr:
            return ::intel_x64::vmcs::guest_ia32_efer::get();

        case ::intel_x64::msrs::ia32_perf_global_ctrl::addr:
            return ::intel_x64::vmcs::guest_ia32_perf_global_ctrl::get_if_exists();

        case ::intel_x64::msrs::ia32_sysenter_cs::addr:
            return ::intel_x64::vmcs::guest_ia32_sysenter_cs::get();

        case ::intel_x64::msrs::ia32_sysenter_esp::addr:
            return ::intel_x64::vmcs::guest_ia32_sysenter_esp::get();

        case ::intel_x64::msrs::ia32_sysenter_eip::addr:
            bfdebug_info(0, "[MAFIA] ia32_sysenter_eip RDMSR");
            return ::intel_x64::vmcs::guest_ia32_sysenter_eip::get();

        case ::intel_x64::msrs::ia32_fs_base::addr:
            return ::intel_x64::vmcs::guest_fs_base::get();

        case ::intel_x64::msrs::ia32_gs_base::addr:
            return ::intel_x64::vmcs::guest_gs_base::get();

        case ::x64::msrs::ia32_lstar::addr:
            bfdebug_info(0, "[MAFIA] ia32_lstar RDMSR");
            return ::x64::msrs::ia32_lstar::get();

        default:
            return ::intel_x64::msrs::get(msr);

        case 0x31:
        case 0x39:
        case 0x1ae:
        case 0x1af:
        case 0x602:
            return 0;
    }
}

void
emulate_wrmsr_mafia(::x64::msrs::field_type msr, ::x64::msrs::value_type val)
{
    switch (msr) {
        case ::intel_x64::msrs::ia32_debugctl::addr:
            ::intel_x64::vmcs::guest_ia32_debugctl::set(val);
            return;

        case ::x64::msrs::ia32_pat::addr:
            ::intel_x64::vmcs::guest_ia32_pat::set(val);
            return;

        case ::intel_x64::msrs::ia32_efer::addr:
            ::intel_x64::vmcs::guest_ia32_efer::set(val);
            return;

        case ::intel_x64::msrs::ia32_perf_global_ctrl::addr:
            ::intel_x64::vmcs::guest_ia32_perf_global_ctrl::set_if_exists(val);
            return;

        case ::intel_x64::msrs::ia32_sysenter_cs::addr:
            ::intel_x64::vmcs::guest_ia32_sysenter_cs::set(val);
            return;

        case ::intel_x64::msrs::ia32_sysenter_esp::addr:
            ::intel_x64::vmcs::guest_ia32_sysenter_esp::set(val);
            return;

        case ::intel_x64::msrs::ia32_sysenter_eip::addr:
            bfdebug_info(0, "[MAFIA] ia32_sysenter_eip WRMSR");
            ::intel_x64::vmcs::guest_ia32_sysenter_eip::set(val);
            return;

        case ::intel_x64::msrs::ia32_fs_base::addr:
            ::intel_x64::vmcs::guest_fs_base::set(val);
            return;

        case ::intel_x64::msrs::ia32_gs_base::addr:
            ::intel_x64::vmcs::guest_gs_base::set(val);
            return;
        case ::x64::msrs::ia32_lstar::addr:
            bfdebug_info(0, "[MAFIA] ia32_lstar WRMSR");
            ::x64::msrs::ia32_lstar::set(val);
            return;

        default:
            ::intel_x64::msrs::set(msr, val);
            return;
    }
}

static bool
handle_rdmsr_mafia(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs)
{
    auto val = emulate_rdmsr_mafia(
                   gsl::narrow_cast<::x64::msrs::field_type>(vmcs->save_state()->rcx)
               );

    vmcs->save_state()->rax = ((val >> 0x00) & 0x00000000FFFFFFFF);
    vmcs->save_state()->rdx = ((val >> 0x20) & 0x00000000FFFFFFFF);

    return advance(vmcs);
}

static bool
handle_wrmsr_mafia(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs)
{
    auto val = 0ULL;

    val |= ((vmcs->save_state()->rax & 0x00000000FFFFFFFF) << 0x00);
    val |= ((vmcs->save_state()->rdx & 0x00000000FFFFFFFF) << 0x20);

    emulate_wrmsr_mafia(
        gsl::narrow_cast<::x64::msrs::field_type>(vmcs->save_state()->rcx),
        val
    );

    return advance(vmcs);
}

static bool
handle_exception_or_non_maskable_interrupt(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs)
{
    uint64_t cr2 = ::intel_x64::cr2::get();
    if(cr2 == mafia::intel_x64::yaju) {
        bfdebug_info(0, "syscall happend!");
        return advance4syscall(vmcs);
    }
    return advance(vmcs);
}

class exit_handler_mafia : public bfvmm::intel_x64::exit_handler
{
public:
    exit_handler_mafia(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs)
    : bfvmm::intel_x64::exit_handler{vmcs}
    {
        using namespace ::intel_x64::vmcs;
        bfdebug_info(0, "mafia hype you");

        add_handler(
            exit_reason::basic_exit_reason::rdmsr,
            handler_delegate_t::create<handle_rdmsr_mafia>()
        );

        add_handler(
            exit_reason::basic_exit_reason::wrmsr,
            handler_delegate_t::create<handle_wrmsr_mafia>()
        );

        add_handler(
            exit_reason::basic_exit_reason::exception_or_non_maskable_interrupt,
            handler_delegate_t::create<mafia::intel_x64::handle_exception_or_non_maskable_interrupt>()
        );
    }
    ~exit_handler_mafia() = default;
};

class mafia_vcpu : public bfvmm::intel_x64::vcpu
{
public:
    mafia_vcpu(vcpuid::type id)
    : bfvmm::intel_x64::vcpu{id}
    {
        // trap page fault
        ::intel_x64::vmcs::exception_bitmap::set((1u << 14));

        // dump vmcs
        ::intel_x64::vmcs::page_fault_error_code_mask::dump(0);
        ::intel_x64::vmcs::page_fault_error_code_match::dump(0);

        // trap page fault when I/D flag is present
        //"the access causing the page-fault exception was an instruction fetch"
        ::intel_x64::vmcs::page_fault_error_code_mask::set((1u << 4));
        ::intel_x64::vmcs::page_fault_error_code_match::set((1u << 4));

        //dump vmcs
        ::intel_x64::vmcs::page_fault_error_code_mask::dump(0);
        ::intel_x64::vmcs::page_fault_error_code_match::dump(0);

        // save original ia32_lstar
        uint64_t ia32_lstar = ::x64::msrs::ia32_lstar::get();
        bfdebug_nhex(0, "lstar", ia32_lstar);
        mafia::intel_x64::original_ia32_lstar[id] = ia32_lstar;

        //change ia32_lstar to MAGIC VALUE
        // so that PF happen when syscall
        //::x64::msrs::ia32_lstar::set(mafia::intel_x64::yaju);

        m_exit_handler_mafia = std::make_unique<mafia::intel_x64::exit_handler_mafia>(vmcs());
    }
    ~mafia_vcpu() = default;
    mafia::intel_x64::exit_handler_mafia *exit_handler()
    { return m_exit_handler_mafia.get(); }
private:
    std::unique_ptr<mafia::intel_x64::exit_handler_mafia> m_exit_handler_mafia;
};
}
}

namespace bfvmm
{
std::unique_ptr<bfvmm::vcpu>
vcpu_factory::make_vcpu(vcpuid::type vcpuid, bfobject *obj)
{
    bfignored(obj);
    return std::make_unique<mafia::intel_x64::mafia_vcpu>(vcpuid);
}
}

