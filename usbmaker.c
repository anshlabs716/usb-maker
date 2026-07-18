#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

// ========================================
// COLORS
// ========================================
#define RED     "\033[0;31m"
#define GREEN   "\033[0;32m"
#define YELLOW  "\033[0;33m"
#define BLUE    "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN    "\033[0;36m"
#define BOLD    "\033[1m"
#define RESET   "\033[0m"

// ========================================
// PACKAGE MANAGER DETECTION
// ========================================
typedef struct {
    char *name;
    char *install_cmd;
    char *update_cmd;
    char *check_cmd;
} PackageManager;

PackageManager managers[] = {
    {"apt", "sudo apt install -y", "sudo apt update", "which apt"},
    {"dnf", "sudo dnf install -y", "sudo dnf check-update", "which dnf"},
    {"yum", "sudo yum install -y", "sudo yum check-update", "which yum"},
    {"pacman", "sudo pacman -S --noconfirm", "sudo pacman -Sy", "which pacman"},
    {"zypper", "sudo zypper install -y", "sudo zypper refresh", "which zypper"},
    {"xbps", "sudo xbps-install -y", "sudo xbps-install -S", "which xbps"},
    {"apk", "sudo apk add", "sudo apk update", "which apk"},
    {"brew", "brew install", "brew update", "which brew"},
    {"pkg", "sudo pkg install -y", "sudo pkg update", "which pkg"},
    {"emerge", "sudo emerge -a", "sudo emerge --sync", "which emerge"},
    {NULL, NULL, NULL, NULL}
};

char *detected_pm = NULL;
char *install_cmd = NULL;

void detect_package_manager() {
    printf("%s[INFO] Detecting package manager...%s\n", BLUE, RESET);

    for (int i = 0; managers[i].name != NULL; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null 2>&1", managers[i].name);
        int result = system(cmd);
        if (result == 0) {
            detected_pm = managers[i].name;
            install_cmd = managers[i].install_cmd;
            printf("%s[+] Detected: %s%s\n", GREEN, detected_pm, RESET);
            return;
        }
    }

    printf("%s[!] No known package manager found%s\n", YELLOW, RESET);
    detected_pm = "unknown";
}

void install_dependency(char *dep) {
    if (detected_pm == NULL || strcmp(detected_pm, "unknown") == 0) {
        printf("%s[!] Unknown package manager. Install %s manually.%s\n", YELLOW, dep, RESET);
        return;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s", install_cmd, dep);
    printf("%s[+] Installing %s...%s\n", BLUE, dep, RESET);
    system(cmd);
}

int check_dependency(char *dep) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null 2>&1", dep);
    return (system(cmd) == 0);
}

void install_missing_deps() {
    char *deps[] = {"dd", "lsblk", "pv", NULL};

    for (int i = 0; deps[i] != NULL; i++) {
        if (!check_dependency(deps[i])) {
            printf("%s[!] %s not found%s\n", YELLOW, deps[i], RESET);
            char response[10];
            printf("Install %s? (y/N): ", deps[i]);
            fgets(response, sizeof(response), stdin);
            if (response[0] == 'y' || response[0] == 'Y') {
                install_dependency(deps[i]);
            }
        }
    }
}

// ========================================
// USB DETECTION
// ========================================
void list_usb_drives() {
    printf("\n%s[+] Available USB Drives:%s\n", GREEN, RESET);
    printf("─────────────────────────────────────────\n");
    system("lsblk -o NAME,SIZE,MODEL,MOUNTPOINT -l | grep -E 'sd.[0-9]|mmcblk' | head -10");
    printf("─────────────────────────────────────────\n\n");
}

char* get_usb_device() {
    static char device[128];
    printf("Enter USB device (e.g., /dev/sdb): ");
    fgets(device, sizeof(device), stdin);
    device[strcspn(device, "\n")] = 0;
    return device;
}

// ========================================
// ISO SELECTION
// ========================================
char* get_iso_path() {
    static char iso[512];
    printf("Enter ISO path: ");
    fgets(iso, sizeof(iso), stdin);
    iso[strcspn(iso, "\n")] = 0;
    return iso;
}

// ========================================
// WRITE ISO
// ========================================
int write_iso(char *iso_path, char *device) {
    // Check if ISO exists
    if (access(iso_path, F_OK) != 0) {
        printf("%s[ERROR] ISO file not found: %s%s\n", RED, iso_path, RESET);
        return 1;
    }

    // Check if device exists
    if (access(device, F_OK) != 0) {
        printf("%s[ERROR] Device not found: %s%s\n", RED, device, RESET);
        return 1;
    }

    // Get ISO size
    char size_cmd[256];
    snprintf(size_cmd, sizeof(size_cmd), "du -h \"%s\" | awk '{print $1}'", iso_path);
    printf("%sISO Size:%s ", YELLOW, RESET);
    system(size_cmd);

    // Unmount if mounted
    printf("%s[+] Unmounting %s...%s\n", BLUE, device, RESET);
    char umount_cmd[256];
    snprintf(umount_cmd, sizeof(umount_cmd), "sudo umount %s* 2>/dev/null", device);
    system(umount_cmd);

    // Warning
    printf("\n%s╔══════════════════════════════════════════════════╗%s\n", RED, RESET);
    printf("%s║  ⚠️  THIS WILL ERASE ALL DATA ON %s %s ║\n", RED, device, RESET);
    printf("%s╚══════════════════════════════════════════════════╝%s\n", RED, RESET);
    printf("Continue? (yes/NO): ");
    char response[10];
    fgets(response, sizeof(response), stdin);
    response[strcspn(response, "\n")] = 0;

    if (strcmp(response, "yes") != 0 && strcmp(response, "y") != 0) {
        printf("%s[!] Aborted.%s\n", YELLOW, RESET);
        return 0;
    }

    // Write ISO
    printf("\n%s[+] Writing ISO to %s...%s\n", BLUE, device, RESET);
    printf("This may take a few minutes...\n\n");

    char write_cmd[512];
    snprintf(write_cmd, sizeof(write_cmd), "sudo dd if=\"%s\" of=%s bs=4M status=progress", iso_path, device);
    int result = system(write_cmd);

    if (result == 0) {
        printf("\n%s[+] Sync...%s\n", BLUE, RESET);
        system("sync");

        printf("\n%s╔══════════════════════════════════════════════════╗%s\n", GREEN, RESET);
        printf("%s║  ✅ Live USB created successfully!            ║%s\n", GREEN, RESET);
        printf("%s╚══════════════════════════════════════════════════╝%s\n", GREEN, RESET);
        return 0;
    } else {
        printf("\n%s[ERROR] Failed to write ISO%s\n", RED, RESET);
        return 1;
    }
}

// ========================================
// BANNER
// ========================================
void print_banner() {
    printf("%s\n", CYAN);
    printf("  _     _     _   _   _____   _____   _____  \n");
    printf(" | |   | |   | | | | |  ___| |  ___| |  _  | \n");
    printf(" | |   | |   | | | | | |__   | |__   | | | | \n");
    printf(" | |   | |   | | | | |  __|  |  __|  | | | | \n");
    printf(" | |___| |___| |_| | | |___  | |___  | |_| | \n");
    printf(" |_________|_____| |_____|  |_____| |_____/  \n");
    printf("%s\n", RESET);
    printf("%sLive USB Creator v1.0%s\n", BOLD, RESET);
    printf("%sBy AnshLabs716%s\n", YELLOW, RESET);
    printf("%s─────────────────────────────────────────%s\n", CYAN, RESET);
}

void show_help() {
    printf("\nUsage: ./liveusb [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -i <iso>     Path to ISO file\n");
    printf("  -d <device>  USB device (e.g., /dev/sdb)\n");
    printf("  -l           List available USB drives\n");
    printf("  -h           Show this help\n");
    printf("  --install    Install dependencies\n\n");
    printf("Examples:\n");
    printf("  ./liveusb -i ubuntu.iso -d /dev/sdb\n");
    printf("  ./liveusb -l\n\n");
}

// ========================================
// MAIN
// ========================================
int main(int argc, char *argv[]) {
    print_banner();

    // Detect package manager
    detect_package_manager();

    // Check dependencies
    printf("%s[+] Checking dependencies...%s\n", BLUE, RESET);
    install_missing_deps();

    // Parse arguments
    char *iso_path = NULL;
    char *device = NULL;
    int list_only = 0;
    int install_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 && i+1 < argc) {
            iso_path = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 && i+1 < argc) {
            device = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0) {
            list_only = 1;
        } else if (strcmp(argv[i], "--install") == 0) {
            install_only = 1;
        } else if (strcmp(argv[i], "-h") == 0) {
            show_help();
            return 0;
        }
    }

    if (install_only) {
        printf("%s[+] Installing all dependencies...%s\n", BLUE, RESET);
        install_missing_deps();
        return 0;
    }

    if (list_only) {
        list_usb_drives();
        return 0;
    }

    // Interactive mode
    if (iso_path == NULL) {
        iso_path = get_iso_path();
    }

    if (device == NULL) {
        list_usb_drives();
        device = get_usb_device();
    }

    if (strlen(iso_path) == 0 || strlen(device) == 0) {
        printf("%s[ERROR] ISO and device required.%s\n", RED, RESET);
        show_help();
        return 1;
    }

    // Write ISO
    return write_iso(iso_path, device);
}
