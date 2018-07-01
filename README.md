# CAUTION
This project is under development.  
This doesn't work properly.

# Bareflank Hypervisor Trap SystemCall
This project is for trapping SystemCall.
## Description
Trap SystemCall for Current Version of Bareflank Hypervisor

## Compilation / Usage

To setup our extension, run the following (assuming Linux):

```
mkdir build; cd build
cmake ../hypervisor -DDEFAULT_VMM=starter_vmm -DEXTENSION=../syscall
make -j<# cores + 1>
```

To test out our extended version of Bareflank, run the following commands:

```
make driver_quick
make quick
```

to get status information, use the following:

```
make status
make dump
```

to reverse this:

```
make unload
make driver_unload
```
