{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  outputs = { self, ... }@inputs: let
    lib = inputs.pkgs.lib or inputs.nixpkgs.lib or (import "${inputs.nixpkgs or <nixpkgs>}/lib");
    forAllSys = lib.genAttrs lib.platforms.all;
    getPkgs = system: overlays: if inputs.pkgs.stdenv.hostPlatform.system or null == system then
      if builtins.isList overlays && overlays != [] then
        inputs.pkgs.appendOverlays overlays
      else
        inputs.pkgs
    else
      import (inputs.pkgs.path or inputs.nixpkgs or <nixpkgs>) {
        inherit system;
        overlays = (if builtins.isList overlays then overlays else []) ++ inputs.pkgs.overlays or [];
        config = inputs.pkgs.config or {};
      };
    mapAttrsToList = f: attrs: builtins.attrValues (builtins.mapAttrs f attrs);
    l_pkg_enum = {
      lua5_1 = "lua51Packages";
      lua5_2 = "lua52Packages";
      lua5_3 = "lua53Packages";
      lua5_4 = "lua54Packages";
      lua5_5 = "lua55Packages";
      luajit = "luajitPackages";
      lua = "luaPackages";
    };
    APPNAME = "tomlua";
    overlay = final: prev: let
      luaCallPackageFn = { toLuaModule, lua, }:
        toLuaModule (lua.stdenv.mkDerivation (finalAttrs: {
          pname = APPNAME;
          version = "${(finalAttrs.passthru.luaModule or lua).luaversion}.dev";
          src = self;
          doCheck = false;
          __structuredAttrs = true;
          strictDeps = true;
          propagatedBuildInputs = [ (finalAttrs.passthru.luaModule or lua) ];
          env = {
            LUA = (finalAttrs.passthru.luaModule or lua).interpreter;
            LUA_INC = (finalAttrs.passthru.luaModule or lua) + "/include";
            BINDIR = placeholder "out" + "/bin";
            LIBDIR = placeholder "out" + "/lib/lua/" + (finalAttrs.passthru.luaModule or lua).luaversion;
            LUADIR = placeholder "out" + "/share/lua/" + (finalAttrs.passthru.luaModule or lua).luaversion;
            LIBFLAG = if (finalAttrs.passthru.luaModule or lua).stdenv.isDarwin then "-bundle -undefined dynamic_lookup" else "-shared";
          };
          meta = {
            mainProgram = "tomlua";
            maintainers = [ lib.maintainers.birdee ];
            license = lib.licenses.mit;
            homepage = "https://github.com/BirdeeHub/tomlua";
            description = "Speedy toml parsing for lua, implemented in C";
          };
        }));
      # lua5_1 = prev.lua5_1.override { packageOverrides };
      l_pkg_main = builtins.mapAttrs (
        n: _: (prev.lib.attrByPath [ n "override" ] null prev) {
          packageOverrides = luaself: luaprev: {
            ${APPNAME} = luaself.callPackage luaCallPackageFn {};
          };
        }
      ) l_pkg_enum;
      # lua51Packages = final.lua5_1.pkgs;
      l_pkg_sets = builtins.listToAttrs (
        mapAttrsToList (
          n: v: {
            name = v;
            value = prev.lib.attrByPath [ n "pkgs" ] null final;
          }
        ) l_pkg_enum
      );
    in l_pkg_main // l_pkg_sets // {
      vimPlugins = prev.vimPlugins // {
        ${APPNAME} = final.vimUtils.toVimPlugin ((final.neovim-unwrapped.lua.pkgs.callPackage luaCallPackageFn {}).overrideAttrs (old: {
          env = (old.env or {}) // rec {
            LUADIR = placeholder "out" + "/lua";
            LIBDIR = LUADIR;
          };
        }));
      };
    };
    packages = forAllSys (system: let
      pkgs = getPkgs system [ overlay ];
    in (
      with builtins; listToAttrs (
        map (n: {
          name = "${n}-${APPNAME}";
          value = pkgs.lib.attrByPath [ n "pkgs" APPNAME ] null pkgs;
        }) (attrNames l_pkg_enum)
      )
    ) // {
      default = pkgs.vimPlugins.${APPNAME};
      "vimPlugins-${APPNAME}" = pkgs.vimPlugins.${APPNAME};
    });
  in {
    overlays.default = overlay;
    inherit packages;
    checks = forAllSys (system: builtins.mapAttrs (_: p: p.overrideAttrs { doCheck = true; }) packages.${system});
    devShells = forAllSys (system: let
      pkgs = getPkgs system [];
      lua = pkgs.luajit.withPackages (lp: [ lp.inspect lp.cjson lp.toml-edit lp.luarocks ]);
    in {
      default = pkgs.mkShell {
        name = "${APPNAME}-dev";
        packages = [ lua ];
        LUA_INCDIR = "${lua}/include";
        LUA = lua.interpreter;
        BEAR = "${pkgs.bear}/bin/bear";
        shellHook = ''
          make clean bear
          [ "$(whoami)" == "birdee" ] && exec zsh
        '';
      };
    });
  };
}
