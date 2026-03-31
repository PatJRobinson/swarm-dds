{
  description = "Pure DDS swarm project in C with Cyclone DDS and make";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
      };
    in {
      devShells.default = pkgs.mkShell {
        packages = with pkgs; [
          gcc
          gnumake
          pkg-config
          bear
          cyclonedds
          gdb
          valgrind
        ];

        shellHook = ''
          echo "DDS swarm dev shell"
          echo "Cyclone DDS available via pkg-config"
          echo
          echo "Try:"
          echo "  pkg-config --cflags --libs cyclonedds"
          echo "  make"
          export CYCLONEDDS_URI=file://$PWD/cyclonedds.xml
        '';
      };
    });
}
