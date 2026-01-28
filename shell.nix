{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  pure = true;
  buildInputs = [
    pkgs.clang
    pkgs.cmake
    pkgs.extra-cmake-modules
    pkgs.python314
    pkgs.jansson
    pkgs.flex
  ];

  # shellHook = ''
  #   ln -sf "$(which clang++)" ./c++
  # '';
}
