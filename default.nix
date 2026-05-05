{ pkgs ? import <nixpkgs> {}, ... }: (import ./flake.nix).outputs {
  self = builtins.path { path = ./.; };
  inherit (pkgs.stdenv.hostPlatform) system;
  inherit pkgs;
}
