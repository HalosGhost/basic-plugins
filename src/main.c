#include "plug.h"

static struct plugin plugins [MAXMODS];

signed
main (signed argc, char * argv []) {

    if ( !argc ) { return EXIT_FAILURE; }

    signed status = EXIT_SUCCESS;

    void * handles [MAXMODS] = { 0 };
    size_t modcount = 0;

    char cwd [PATH_MAX] = "";
    errno = 0;
    char * res = getcwd(cwd, PATH_MAX - 1);
    if ( !res ) {
        fprintf(stderr, "Failed to get cwd: %s\n", strerror(errno));
    }

    char * basepath = argc > 1 && argv[1] ? argv[1] : dirname(argv[0]);

    size_t modpathlen = strlen(basepath) + sizeof "/modules";
    char * modpath = malloc(modpathlen);

    snprintf(modpath, modpathlen, "%s/modules", basepath);

    errno = 0;
    signed res1 = chdir(modpath);
    if ( res1 ) {
        fprintf(stderr, "Failed to cd to %s: %s\n", modpath, strerror(errno));
    }

    free(modpath);

    DIR * modules = opendir(".");
    if ( !modules ) {
        fprintf(stderr, "Failed to open modules directory: %s\n", strerror(errno));
        status = EXIT_FAILURE;
        goto cleanup;
    }

    for ( struct dirent * p = readdir(modules); p; p = readdir(modules) ) {
        if ( modcount == MAXMODS ) { break; }

        size_t len = strlen(p->d_name);
        if ( !strncmp(".so", p->d_name + len - 3, 3) ) {
            size_t pathlen = len + sizeof "./" + 1;
            char * path = malloc(pathlen);
            snprintf(path, pathlen, "./%s", p->d_name);
            handles[modcount] = dlopen(path, RTLD_LAZY);
            if ( !handles[modcount] ) {
                fprintf(stderr, "Failed to load module: %s\n", dlerror());
            } else {
                ++modcount;
            }

            free(path);
        }
    }

    fprintf(stderr, "Loaded %zu module(s)\n", modcount);

    for ( size_t i = 0; i < modcount; ++ i ) {
        plugins[i] = load_plugin(handles[i]);
    }

    qsort(plugins, modcount, sizeof (struct plugin), compare_plugins);

    for ( size_t i = 0; i < modcount; ++ i ) {
        if ( !plugins[i].priority ) { continue; }

        if ( plugins[i].setup ) {
            if ( !plugins[i].setup() ) { plugins[i].priority = 0; continue; }
        }
    }

    for ( size_t i = 0; i < modcount; ++ i ) {
        if ( !plugins[i].priority ) { continue; }

        if ( !plugins[i].play(&plugins[i].buffer) ) { continue; }
        printf("%s%s", plugins[i].buffer, i + 1 != modcount ? MODSEP : "");
    }

    printf("\n");

    for ( size_t i = 0; i < modcount; ++ i ) {
        if ( plugins[i].teardown ) { plugins[i].teardown(); }
        free(plugins[i].buffer);
    }

    cleanup:
        (void)chdir(cwd);
        if ( modules ) { closedir(modules); }

        for ( size_t i = 0; i < modcount; ++ i ) {
            if ( handles[i] ) { dlclose(handles[i]); }
        }

        return status;
}