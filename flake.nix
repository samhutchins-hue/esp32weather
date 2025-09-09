# flake.nix
{
  description = "A Nix flake for ESP32 development with PlatformIO";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pkgs.mkShell {
          name = "platformio-esp32-shell";

          # The packages available in the development environment.
          buildInputs = with pkgs; [
            # The core PlatformIO CLI tool.
            platformio
          ];

          # This hook runs when you enter the shell with `nix develop`.
          shellHook = ''
            echo "✅ Entering PlatformIO ESP32 dev environment..."
            # Set PLATFORMIO_CORE_DIR to a local directory.
            # This is the key to making PlatformIO work cleanly with Nix.
            # It prevents PlatformIO from polluting ~/.platformio and makes
            # your project's dependencies self-contained.
            export PLATFORMIO_CORE_DIR="$(pwd)/.platformio"
            echo "⚡️ Ready to build! Use 'platformio run' or 'pio run -t upload'."
          '';
        };
      }
    );
}
