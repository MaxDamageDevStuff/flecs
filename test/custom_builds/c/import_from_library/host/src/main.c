#include <host.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#define LIB_NAME "physics_plugin.dll"
#define EXE_PATH_BUF_SIZE 1024
#define PLUGIN_PATH_BUF_SIZE 4096
#define PATH_SEP '\\'
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#define LIB_NAME "libphysics_plugin.dylib"
#define EXE_PATH_BUF_SIZE 1024
#define PLUGIN_PATH_BUF_SIZE PATH_MAX
#define PATH_SEP '/'
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#define LIB_NAME "libphysics_plugin.so"
#define EXE_PATH_BUF_SIZE 1024
#define PLUGIN_PATH_BUF_SIZE PATH_MAX
#define PATH_SEP '/'
#else
#error "unsupported platform"
#endif

static char plugin_path[PLUGIN_PATH_BUF_SIZE];

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int resolve_plugin_path(void) {
    char exe_path[EXE_PATH_BUF_SIZE];

#if defined(_WIN32)
    DWORD n = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (n == 0 || n >= sizeof(exe_path)) {
        fprintf(stderr, "GetModuleFileNameA failed\n");
        return -1;
    }
#elif defined(__APPLE__)
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        fprintf(stderr, "failed to resolve executable path\n");
        return -1;
    }
#elif defined(__linux__)
    ssize_t n = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (n < 0) {
        fprintf(stderr, "readlink /proc/self/exe failed\n");
        return -1;
    }
    exe_path[n] = '\0';
#endif

    char *exe_end = strrchr(exe_path, PATH_SEP);
    if (!exe_end) {
        fprintf(stderr, "could not derive directory from '%s'\n", exe_path);
        return -1;
    }
    *exe_end = '\0';

    char *arch_cfg = strrchr(exe_path, PATH_SEP);
    if (!arch_cfg) {
        fprintf(stderr, "could not derive arch-cfg from '%s'\n", exe_path);
        return -1;
    }
    arch_cfg++;

    snprintf(plugin_path, sizeof(plugin_path),
        "%s/../../../physics_plugin/bin/%s/%s",
        exe_path, arch_cfg, LIB_NAME);
    if (file_exists(plugin_path)) {
        return 0;
    }

    snprintf(plugin_path, sizeof(plugin_path),
        "%s/../../physics_plugin/%s/%s",
        exe_path, arch_cfg, LIB_NAME);
    if (file_exists(plugin_path)) {
        return 0;
    }

    fprintf(stderr, "plugin library not found near '%s'\n", exe_path);
    return -1;
}

static char* test_module_to_dl(const char *module) {
    (void)module;
    return ecs_os_strdup(plugin_path);
}

int main(int argc, char *argv[]) {
    if (resolve_plugin_path() != 0) {
        return 1;
    }

    ecs_os_api.module_to_dl_ = test_module_to_dl;
    ecs_set_os_api_impl();

    ecs_world_t *world = ecs_init_w_args(argc, argv);

    ecs_entity_t e = ecs_import_from_library(world, "physics.plugin", NULL);
    if (!e) {
        fprintf(stderr, "ecs_import_from_library returned 0\n");
        return 1;
    }

    if (e != ecs_lookup(world, "physics.plugin")) {
        fprintf(stderr, "module entity does not match path lookup\n");
        return 1;
    }

    return ecs_fini(world);
}
