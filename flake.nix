{
  description = "NipoVPN - Powerful HTTP proxy";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachSystem ["x86_64-linux" "aarch64-linux"] (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      nipovpn = pkgs.stdenv.mkDerivation {
        pname = "nipovpn";
        version = "1.1.56";
        src = self;

        nativeBuildInputs = with pkgs; [cmake];
        buildInputs = with pkgs; [boost yaml-cpp openssl];

        cmakeFlags = ["-DCMAKE_BUILD_TYPE=Release"];

        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin $out/etc/nipovpn
          cp core/nipovpn $out/bin/nipovpn
          cp -r $src/nipovpn/etc/nipovpn/. $out/etc/nipovpn/
          runHook postInstall
        '';

        meta = with pkgs.lib; {
          description = "Powerful HTTP proxy with traffic obfuscation";
          homepage = "https://github.com/MortezaBashsiz/nipovpn";
          license = licenses.gpl3Plus;
          platforms = platforms.linux;
          mainProgram = "nipovpn";
        };
      };
    in {
      packages.default = nipovpn;
      packages.nipovpn = nipovpn;

      devShells.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          cmake
          ninja
          boost
          yaml-cpp
          openssl
          clang-tools
          gdb
          valgrind
        ];

        shellHook = ''
          echo "NipoVPN dev environment ready"
          echo ""
          echo "You can build using either Make or Ninja:"
          echo "  Standard (Make):"
          echo "    cmake -B build -DCMAKE_BUILD_TYPE=Debug"
          echo "    cmake --build build -j\$(nproc)"
          echo ""
          echo "  Fast (Ninja):"
          echo "    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug"
          echo "    cmake --build build -j\$(nproc)"
        '';
      };
    });
}
