<div align="center">

# QLauncher

**Play Minecraft your way.** A fast, open-source custom launcher based on [PollyMC-Continued](https://github.com) and [Prism Launcher](https://github.com).

[![GitHub Repo stars](https://shields.io)](https://github.com)
[![GitHub release](https://shields.io)](https://github.com)

</div>

## Features

- **Accounts:** Offline accounts without Microsoft login requirements, authlib-injector support, and a first-run setup wizard.
- **Skins:** Local skin library management and integrated [crafty.gg](https://crafty.gg) skin browser with 3D previews.
- **Tools:** Command palette (`Ctrl+Shift+P`), bot manager for scripting and controlling Minecraft bots, NeoForge Jar-in-Jar instance health checks, and a built-in bug reporter.
- **Polish:** Enhanced settings search, automatic language selection, GitHub releases auto-updates (Windows/Linux), and APT/Pacman repositories via GitHub Pages.

## Installing on NixOS

For NixOS, use the flake installation method:

```shell
nix profile add github:shotti/launcher
```

Or add it to your flake's `inputs`:

```nix
{
  inputs = {
    qlauncher.url = "github:shotti/launcher";
  };
}
```

See [nix/README.md](nix/README.md) for custom packaging and binary cache details.

## Contributing

Open an [issue](https://github.com) or pull request on GitHub, or use the in-app bug reporter.

## License

Open-source and released under the project [LICENSE](https://github.com).

