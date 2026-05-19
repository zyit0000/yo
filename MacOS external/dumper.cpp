#include <iostream>
#include <vector>
#include <string>
#include <mach/mach.h>
#include <mach-o/dyld_images.h>
#include <sys/sysctl.h>
#include <libproc.h>
#include <unistd.h>

// Helper to get PID by process name
pid_t get_pid_by_name(const char* name) {
    pid_t pids[2048];
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
    int n_pids = bytes / sizeof(pids[0]);

    for (int i = 0; i < n_pids; i++) {
        struct proc_bsdinfo proc;
        int st = proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &proc, proc_bsdinfo::PROC_PIDTBSDINFO_SIZE);
        if (st == proc_bsdinfo::PROC_PIDTBSDINFO_SIZE) {
            if (strcmp(name, proc.pbi_name) == 0 || strstr(proc.pbi_comm, name) != nullptr) {
                return pids[i];
            }
        }
    }
    return 0;
}

// Helper to read memory
bool read_memory(mach_port_t task, mach_vm_address_t address, void* buffer, mach_vm_size_t size) {
    mach_vm_size_t read_size = 0;
    kern_return_t kr = mach_vm_read_overwrite(task, address, size, (mach_vm_address_t)buffer, &read_size);
    return (kr == KERN_SUCCESS && read_size == size);
}

// Read an integer (pointer size)
uintptr_t read_ptr(mach_port_t task, mach_vm_address_t address) {
    uintptr_t val = 0;
    read_memory(task, address, &val, sizeof(val));
    return val;
}

int main() {
    std::cout << "[*] macOS External Dumper Starting...\n";

    pid_t pid = get_pid_by_name("RobloxPlayer");
    if (pid == 0) {
        std::cout << "[-] RobloxPlayer not found. Is it running?\n";
        return 1;
    }
    std::cout << "[+] Found RobloxPlayer PID: " << pid << "\n";

    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        std::cout << "[-] task_for_pid failed (" << kr << "). Try running with sudo.\n";
        return 1;
    }
    std::cout << "[+] Obtained task port: " << task << "\n";

    // Basic logic to find base address of the main module (RobloxPlayer)
    task_dyld_info dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    kr = task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count);
    
    if (kr != KERN_SUCCESS) {
        std::cout << "[-] Failed to get task info.\n";
        return 1;
    }

    struct dyld_all_image_infos infos;
    if (!read_memory(task, dyld_info.all_image_info_addr, &infos, sizeof(infos))) {
        std::cout << "[-] Failed to read dyld_all_image_infos.\n";
        return 1;
    }

    mach_vm_address_t image_array_addr = (mach_vm_address_t)infos.infoArray;
    struct dyld_image_info image;
    
    // Read the first image (usually the executable itself)
    if (!read_memory(task, image_array_addr, &image, sizeof(image))) {
         std::cout << "[-] Failed to read first dyld_image_info.\n";
         return 1;
    }

    mach_vm_address_t base_address = (mach_vm_address_t)image.imageLoadAddress;
    std::cout << "[+] Roblox Base Address: 0x" << std::hex << base_address << std::dec << "\n";

    // TODO: AOB Scan for TaskScheduler or other offsets based on signatures
    // For now, this is just a skeleton demonstrating the mach API access.
    std::cout << "[*] Scanning for TaskScheduler... (Implementation needed based on patterns)\n";

    // Example reading a few bytes from base
    uint32_t magic = 0;
    if (read_memory(task, base_address, &magic, sizeof(magic))) {
        std::cout << "[+] Magic at base: 0x" << std::hex << magic << std::dec << "\n";
        if (magic == 0xfeedfacf) { // Mach-O 64-bit magic
             std::cout << "    -> Valid Mach-O 64-bit header\n";
        }
    }

    std::cout << "[+] Done.\n";

    return 0;
}
