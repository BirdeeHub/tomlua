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
    }: runCommandCC APPNAME {
      LUA_INCDIR = "${lua}/include";
      DESTDIR = "${placeholder "out"}/lib";
    } "cd ${builtins.path { path = ./.; }} && make build";
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
      lua = pkgs.luajit.withPackages (lp: [lp.inspect lp.cjson lp.toml-edit]);
    in {
      default = pkgs.mkShell {
        name = "tomlua-dev";
        packages = [ lua pkgs.luarocks pkgs.bear ];
        inputsFrom = [ ];
        LUA_INCDIR = "${lua}/include";
        DESTDIR = "./lib";
        shellHook = ''
          make bear
          export LUA_CPATH="./lib/?.so;$LUA_CPATH"
          exec zsh
        '';
      };
    });
  };
}
