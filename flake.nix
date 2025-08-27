{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  };
  outputs = {self, nixpkgs}: let
    forAllSys = nixpkgs.lib.genAttrs nixpkgs.lib.platforms.all;
    APPNAME = "tomlua";
    app = {
      lua,
      runCommandCC
    , ...
    }: runCommandCC APPNAME {} ''
      mkdir -p "$out"
      $CC -O3 -fPIC -shared -o "$out/${APPNAME}.so" '${./src/tomlua.c}' '${./src/str_buf.c}' '${./src/parse_str.c}' '${./src/parse_keys.c}' '${./src/parse.c}' -I'${lua}/include'
    '';
  in {
    overlays.default = final: prev: {
      ${APPNAME} = prev.callPackage app { lua = prev.luajit; };
    };
    packages = forAllSys (system: let
      pkgs = import nixpkgs { inherit system; overlays = [ self.overlays.default ]; };
    in {
      default = pkgs.${APPNAME};
      ${APPNAME} = pkgs.${APPNAME};
    });
    devShells = forAllSys (system: let
      pkgs = import nixpkgs { inherit system; overlays = [ self.overlays.default ]; };
      lua = pkgs.luajit.withPackages (lp: [lp.inspect lp.cjson]);
      devbuild = pkgs.writeShellScriptBin "build_cc" ''
        mkdir -p ./build
        ${pkgs.bear}/bin/bear -- $CC -O3 -fPIC -shared -o ./build/tomlua.so ./src/tomlua.c ./src/str_buf.c ./src/parse_str.c ./src/parse_keys.c ./src/parse.c -I"${lua}/include"
      '';
    in {
      default = pkgs.mkShell {
        name = "tomlua-dev";
        packages = [ lua devbuild ];
        inputsFrom = [ ];
        shellHook = ''
          build_cc
          export LUA_CPATH="./build/?.so;$LUA_CPATH"
          exec zsh
        '';
      };
    });
  };
}
