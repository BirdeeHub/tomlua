{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  outputs = {self, nixpkgs}: let
    forAllSys = nixpkgs.lib.genAttrs nixpkgs.lib.platforms.all;
    inherit (nixpkgs) lib;
    APPNAME = "tomlua";
    l_pkg_enum = {
      lua5_1 = "lua51Packages";
      lua5_2 = "lua52Packages";
      lua5_3 = "lua53Packages";
      lua5_4 = "lua54Packages";
      luajit = "luajitPackages";
      lua = "luaPackages";
    };
    overlay = final: prev: let
      # lua5_1 = prev.lua5_1.override { packageOverrides };
      l_pkg_main = builtins.mapAttrs (n: _: (lib.attrByPath [ n "override" ] null prev) {
        packageOverrides = luaself: luaprev: {
          ${APPNAME} = luaself.callPackage ({buildLuarocksPackage}:
            buildLuarocksPackage {
              pname = APPNAME;
              version = "scm-1";
              knownRockspec = "${self}/${APPNAME}-scm-1.rockspec";
              src = self;
            }) {};
        };
      }) l_pkg_enum;
      # lua51Packages = final.lua5_1.pkgs;
      l_pkg_sets = builtins.listToAttrs (lib.mapAttrsToList (n: v: {
        name = v;
        value = lib.attrByPath [ n "pkgs" ] null final;
      }) l_pkg_enum);
    in l_pkg_main // l_pkg_sets // {
      vimPlugins = prev.vimPlugins // {
        ${APPNAME} = final.neovimUtils.buildNeovimPlugin {
          pname = APPNAME;
          version = "dev";
          src = self;
        };
      };
    };
  in {
    overlays.default = overlay;
    packages = forAllSys (system: let
      pkgs = import (nixpkgs.path or nixpkgs) { inherit system; overlays = [ overlay ]; };
    in (with builtins; lib.pipe l_pkg_enum [
      attrNames
      (map (n: {
        name = "${n}-${APPNAME}";
        value = lib.attrByPath [ n "pkgs" APPNAME ] null pkgs;
      }))
      listToAttrs
    ]) // {
      default = pkgs.vimPlugins.${APPNAME};
      "vimPlugins-${APPNAME}" = pkgs.vimPlugins.${APPNAME};
    });
    devShells = forAllSys (system: let
      pkgs = import (nixpkgs.path or nixpkgs) { inherit system; overlays = [ overlay ]; };
      lua = pkgs.luajit.withPackages (lp: [lp.inspect lp.cjson lp.toml-edit]);
    in {
      default = pkgs.mkShell {
        name = "${APPNAME}-dev";
        packages = [ lua pkgs.luarocks ];
        LUA_INCDIR = "${lua}/include";
        LUA = lua.interpreter;
        GREP = "${pkgs.gnugrep}/bin/grep";
        BEAR = "${pkgs.bear}/bin/bear";
        shellHook = ''
          make clean build bear
          [ "$(whoami)" == "birdee" ] && exec zsh
        '';
      };
    });
  };
}
