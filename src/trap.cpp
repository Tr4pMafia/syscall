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

void dump_regs(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs) noexcept
{
    bferror_info(0, "dump regs");
    bferror_subnhex(0, "rax", vmcs->save_state()->rax);
    bferror_subnhex(0, "rbx", vmcs->save_state()->rbx);
    bferror_subnhex(0, "rcx", vmcs->save_state()->rcx);
    bferror_subnhex(0, "rdx", vmcs->save_state()->rdx);
    bferror_subnhex(0, "rbp", vmcs->save_state()->rbp);
    bferror_subnhex(0, "rsi", vmcs->save_state()->rsi);
    bferror_subnhex(0, "rdi", vmcs->save_state()->rdi);
    bferror_subnhex(0, "r08", vmcs->save_state()->r08);
    bferror_subnhex(0, "r09", vmcs->save_state()->r09);
    bferror_subnhex(0, "r10", vmcs->save_state()->r10);
    bferror_subnhex(0, "r11", vmcs->save_state()->r11);
    bferror_subnhex(0, "r12", vmcs->save_state()->r12);
    bferror_subnhex(0, "r13", vmcs->save_state()->r13);
    bferror_subnhex(0, "r14", vmcs->save_state()->r14);
    bferror_subnhex(0, "r15", vmcs->save_state()->r15);
    bferror_subnhex(0, "rip", vmcs->save_state()->rip);
    bferror_subnhex(0, "rsp", vmcs->save_state()->rsp);
}

static bool
handle_syscall(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs)
{

    bfdebug_info(0, "[MAFIA] system call happend!");
    dump_regs(vmcs);
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
            handler_delegate_t::create<mafia::intel_x64::handle_syscall>()
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

