def _staged_install_tree_impl(ctx):
    out = ctx.actions.declare_directory(ctx.label.name + "_root")

    enblend = ctx.executable.enblend.path
    enfuse = ctx.executable.enfuse.path
    enblend_man = ctx.file.enblend_man.path
    enfuse_man = ctx.file.enfuse_man.path

    script = """
        set -euo pipefail
        OUT="{out}"
        rm -rf "$OUT"
        mkdir -p "$OUT/bin"
        mkdir -p "$OUT/share/man/man1"
        install -m755 "{enblend}" "$OUT/bin/enblend"
        install -m755 "{enfuse}" "$OUT/bin/enfuse"
        install -m644 "{enblend_man}" "$OUT/share/man/man1/enblend.1"
        install -m644 "{enfuse_man}" "$OUT/share/man/man1/enfuse.1"
    """.format(
        out = out.path,
        enblend = enblend,
        enfuse = enfuse,
        enblend_man = enblend_man,
        enfuse_man = enfuse_man,
    )

    ctx.actions.run_shell(
        inputs = [
            ctx.executable.enblend,
            ctx.executable.enfuse,
            ctx.file.enblend_man,
            ctx.file.enfuse_man,
        ],
        outputs = [out],
        command = script,
        progress_message = "Staging install tree for {}".format(ctx.label.name),
    )

    return [DefaultInfo(files = depset([out]))]

staged_install_tree = rule(
    implementation = _staged_install_tree_impl,
    attrs = {
        "enblend": attr.label(
            executable = True,
            cfg = "target",
            mandatory = True,
        ),
        "enfuse": attr.label(
            executable = True,
            cfg = "target",
            mandatory = True,
        ),
        "enblend_man": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "enfuse_man": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
    },
    doc = "Stages the install tree produced by the Bazel build.",
)
