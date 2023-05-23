{
  description = "A Nix-flake-based C development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/release-22.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    { self
    , nixpkgs
    , flake-utils
    }:

    flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.default = pkgs.mkShellNoCC {
        packages = with pkgs; [
            which
            htop
            gcc
            libedit
        ];

        shellHook = ''
          echo "Welcome to clisp" 
        '';
      };
    });
}
