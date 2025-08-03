# Root BUILD file for enblend-enfuse project

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:public"])

# Configuration header (you'll need to generate this based on your system)
genrule(
    name = "generate_config_h",
    outs = ["config.h"],
    cmd = """
cat > $@ << 'EOF'
#ifndef CONFIG_H
#define CONFIG_H

/* Package version */
#define PACKAGE_VERSION "4.2"
#define PACKAGE_NAME "enblend-enfuse"
#define PACKAGE_STRING "enblend-enfuse 4.2"

/* Enable features */
#define HAVE_LIBGSL 1
#define HAVE_LIBJPEG 1
#define HAVE_LIBPNG 1
#define HAVE_LIBTIFF 1
#define HAVE_LIBLCMS2 1
#define HAVE_BOOST 1

/* Optional features - uncomment to enable */
// #define HAVE_LIBEXIV2 1
// #define HAVE_OPENEXR 1
// #define HAVE_OPENCL 1
// #define OPENMP 1

/* System features */
#define HAVE_FENV_H 1
#define HAVE_UNISTD_H 1
#define HAVE_LRINT 1
#define HAVE_LRINTF 1
#define HAVE_MKSTEMP 1

/* Paths */
#define DEFAULT_OPENCL_PATH "/usr/local/share/enblend/kernels:/usr/share/enblend/kernels"

#endif /* CONFIG_H */
EOF
""",
)

# VERSION file content
genrule(
    name = "generate_version",
    outs = ["VERSION"],
    cmd = 'echo "4.2" > $@',
)

# Main BUILD targets are in src/BUILD
