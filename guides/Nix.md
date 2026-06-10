## Nix Guide

**Note:** Nix Flakes are an experimental feature and must be enabled before using any of the commands in this guide.

### Enable Flakes

* **NixOS:** Follow the instructions in the NixOS & Flakes Book:

  [Enabling NixOS with Flakes](https://nixos-and-flakes.thiscute.world/nixos-with-flakes/nixos-with-flakes-enabled?utm_source=chatgpt.com#enable-nix-flakes)

* **Non-NixOS systems:** Add the following to `~/.config/nix/nix.conf` (or `/etc/nix/nix.conf`):

```bash
experimental-features = nix-command flakes
```

## Installation

### Install into your Nix profile

```bash
nix profile install github:MortezaBashsiz/nipovpn
```

### NixOS (system-wide installation)

Add the flake as an input in your system `flake.nix`:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    nipovpn.url = "github:MortezaBashsiz/nipovpn";
  };

  outputs = { self, nixpkgs, nipovpn, ... }: {
    nixosConfigurations.yourhostname = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux"; # or "aarch64-linux"

      modules = [
        ./configuration.nix
        {
          environment.systemPackages = [
            nipovpn.packages.${pkgs.system}.default
          ];
        }
      ];
    };
  };
}
```

## Building

### Manual Build (without Nix)

For traditional builds on Linux, please refer to [`BuildLinux.md`](./BuildLinux.md).

### Build with Nix

From the repository root:

```bash
nix build
```

Or directly from GitHub without cloning:

```bash
nix build github:MortezaBashsiz/nipovpn
```

## Development

Enter the development shell with all dependencies pre-configured:

```bash
git clone https://github.com/MortezaBashsiz/nipovpn.git
cd nipovpn

nix develop
```

If the command completes successfully, the development environment is ready to use.