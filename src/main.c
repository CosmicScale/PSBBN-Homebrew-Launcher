#include "include/main.h"

char cwd[256];     // Buffer to hold current working directory
char elfFile[256];

// Function to load modules for HDD access
static void LoadModules()
{
    SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL, NULL);
    SifExecModuleBuffer(iomanx_irx, size_iomanx_irx, 0, NULL, NULL);
    SifExecModuleBuffer(filexio_irx, size_filexio_irx, 0, NULL, NULL);
    fileXioInit();
    SifExecModuleBuffer(ps2atad_irx, size_ps2atad_irx, 0, NULL, NULL);
    SifExecModuleBuffer(ps2hdd_irx, size_ps2hdd_irx, 0, NULL, NULL);
    SifExecModuleBuffer(ps2fs_irx, size_ps2fs_irx, 0, NULL, NULL);
}

// Funtion for returning to OSD when error is encountered
static inline void BootError(void)
{
    SifExitRpc();
    char *args[2] = {"BootError", NULL};
    ExecOSD(1, args);
}

// Function to find specific .elf files
static int FindElfFile(char *elfFile, size_t elfFileSize)
{
    DIR *d;
    struct dirent *dir;
    int found = 0;

    d = opendir("pfs0:/");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcasecmp(dir->d_name, "OPL") == 0) {
                strncpy(elfFile, "OPNPS2LD.ELF", elfFileSize - 1);
                elfFile[elfFileSize - 1] = '\0'; // Ensure null-termination
                found = 1;
                break;
            }
            if (strcasecmp(dir->d_name, "NHDDL") == 0) {
                strncpy(elfFile, "nhddl.elf", elfFileSize - 1);
                elfFile[elfFileSize - 1] = '\0'; // Ensure null-termination
                found = 1;
                break;
            }
        }
        closedir(d);
    }

    if (!found) {
        BootError();
        return -1; // Failure
    }

    return 0; // Success
}

int main(int argc, char *argv[])
{

    // Initialize and configure enviroment
    if (argc > 1) {
        while (!SifIopReset(NULL, 0)) {}
        while (!SifIopSync()) {}
        SifInitRpc(0);
    }

    SifInitIopHeap();
    SifLoadFileInit();
    sbv_patch_enable_lmb();
    LoadModules();
    SifLoadFileExit();
    SifExitIopHeap();

    // Get the current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        BootError();
        return 1;
    }

    // Remove ":pfs:" from the end of the current working directory path
    char *pfs_pos = strstr(cwd, ":pfs:");
    if (pfs_pos) {
        *pfs_pos = '\0'; // Terminate the string before ":pfs:"
    }

    // Mount the current working directory
    if (fileXioMount("pfs0:", cwd, FIO_MT_RDWR) != 0) {
        return 1;
    }

    // Search for NHDDL or OPL file
    if (FindElfFile(elfFile, sizeof(elfFile)) != 0) {
        BootError();
        return 1;
    }

    // Unmount the HDD partition
    fileXioUmount("pfs0:");

    if (fileXioMount("pfs0:", "hdd0:+OPL", FIO_MT_RDWR) != 0) {
        return 1;
    }

    char fullFilePath[512];
    snprintf(fullFilePath, sizeof(fullFilePath), "pfs0:/%s", elfFile);
    if (LoadELFFromFile(fullFilePath, 0, NULL) != 0) {
        BootError();
        return 1;
    }

    return 0;
}
