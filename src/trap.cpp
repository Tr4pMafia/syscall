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

::x64::msrs::value_type
emulate_rdmsr(::x64::msrs::field_type msr)
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
            return ::intel_x64::vmcs::guest_ia32_sysenter_eip::get();

        case ::intel_x64::msrs::ia32_fs_base::addr:
            return ::intel_x64::vmcs::guest_fs_base::get();

        case ::intel_x64::msrs::ia32_gs_base::addr:
            return ::intel_x64::vmcs::guest_gs_base::get();

        case ::intel_x64::msrs::ia32_lstar::addr:
            bfdebug_info(0, "[MAFIA] ia32_lstar RDMSR");
            return ::intel_x64::vmcs::ia32_lstar::get();

        default:
            return ::intel_x64::msrs::get(msr);

        // QUIRK:
        //
        // The following is specifically for CPU-Z. For whatever reason, it is
        // reading the following undefined MSRs, which causes the system to
        // freeze since attempting to read these MSRs in the exit handler
        // will cause a GPF which is not being caught. The result is, the core
        // that runs RDMSR on these freezes, the other cores receive an
        // INIT signal to reset, and the system dies.
        //

        case 0x31:
        case 0x39:
        case 0x1ae:
        case 0x1af:
        case 0x602:
            return 0;
    }
}

void
emulate_wrmsr(::x64::msrs::field_type msr, ::x64::msrs::value_type val)
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
            ::intel_x64::vmcs::guest_ia32_sysenter_eip::set(val);
            return;

        case ::intel_x64::msrs::ia32_fs_base::addr:
            ::intel_x64::vmcs::guest_fs_base::set(val);
            return;

        case ::intel_x64::msrs::ia32_gs_base::addr:
            ::intel_x64::vmcs::guest_gs_base::set(val);
            return;
        case ::intel_x64::msrs::ia32_lstar::addr:
            bfdebug_info(0, "[MAFIA] ia32_lstar WRMSR");
            ::intel_x64::vmcs::ia32_lstar::set(val);
            return;

        default:
            ::intel_x64::msrs::set(msr, val);
            return;
    }
}

class exit_handler_mafia : public bfvmm::intel_x64::exit_handler
{
public:
    exit_handler_mafia(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs)
    : bfvmm::intel_x64::exit_handler{vmcs}
    {
        using namespace ::intel_x64::vmcs;
        bfdebug_info(0, "mafia hype you");
        /*
        add_handler(
            exit_reason::basic_exit_reason::rdmsr,
            handler_delegate_t::create<mafia::intel_x64::handle_syscall>()
        );*/
    }
    ~exit_handler_mafia() = default;
};

class mafia_vcpu : public bfvmm::intel_x64::vcpu
{
public:
    mafia_vcpu(vcpuid::type id)
    : bfvmm::intel_x64::vcpu{id}
    {
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

