{
  description = "C64-inspired BASIC interpreter";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
    in {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "basic";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [ pkgs.gnumake ];
            buildPhase = ''
              make build/basic
            '';
            installPhase = ''
              mkdir -p $out/bin
              cp build/basic $out/bin/basic
            '';
          };
        });

      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            packages = [
              pkgs.clang-tools
              pkgs.gcc
              pkgs.gnumake
              pkgs.python3
            ]
            ++ pkgs.lib.optional (!pkgs.stdenv.isDarwin) pkgs.valgrind;
            shellHook = ''
            '';
          };
        });
    };
}
