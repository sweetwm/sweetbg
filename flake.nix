{
  description = "Sweetbg - small static Wayland wallpaper daemon";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    {
      self,
      nixpkgs,
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: {
        sweetbg = pkgs.callPackage ./nix/package.nix { };
        default = self.packages.${pkgs.stdenv.hostPlatform.system}.sweetbg;
      });

      overlays.default = final: _prev: {
        sweetbg = final.callPackage ./nix/package.nix { };
      };

      # nix develop: meson, ninja, and the C libraries
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.sweetbg ];
          # The -O0 debug gates use -Werror, and fortify warns without -O
          hardeningDisable = [ "fortify" ];
        };
      });

      formatter = forAllSystems (pkgs: pkgs.alejandra);
    };
}
