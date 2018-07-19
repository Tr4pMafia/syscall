//
// Bareflank Hypervisor Trap SystemCall
// Copyright (C) 2018 morimolymoly (Tr4pMafia)
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include <bfvmm/hve/arch/intel_x64/vcpu/vcpu.h>
#include <bfvmm/vcpu/vcpu.h>
#include <bfdebug.h>
#include <bfvmm/vcpu/vcpu_factory.h>
#include <bfgsl.h>
#include <bfstring.h>
#include <string>
#include <vector>
#include<consts.h>

namespace mafia
{
namespace intel_x64
{
static std::vector<uint64_t> original_ia32_lstar(MAX_VCPU_NUM);

bool
advance4syscall(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs) noexcept
{
    vmcs->save_state()->rip = mafia::intel_x64::original_ia32_lstar[vmcs->save_state()->vcpuid];
    return true;
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

static bool
handle_init_signal(gsl::not_null<bfvmm::intel_x64::vmcs *> vmcs)
{
    // [NOTE] do nothing here, but is it correct??
    return advance(vmcs);
}

class mafia_vcpu : public bfvmm::intel_x64::vcpu
{
public:
    mafia_vcpu(vcpuid::type id)
    : bfvmm::intel_x64::vcpu{id}
    {
        exit_handler()->add_handler(
            ::intel_x64::vmcs::exit_reason::basic_exit_reason::exception_or_non_maskable_interrupt,
            handler_delegate_t::create<mafia::intel_x64::handle_exception_or_non_maskable_interrupt>()
        );

        exit_handler()->add_handler(
            ::intel_x64::vmcs::exit_reason::basic_exit_reason::init_signal,
            handler_delegate_t::create<mafia::intel_x64::handle_init_signal>()
        );

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
    }
    ~mafia_vcpu() = default;
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

