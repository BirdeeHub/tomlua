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
      luaCallPackageFn = { buildLuarocksPackage, }:
        buildLuarocksPackage {
          pname = APPNAME;
          version = "scm-1";
          src = self;
          postConfigure = ''
            chmod +w "$rockspecFilename"
            echo "deploy = { wrap_bin_scripts = false, }" >> "$rockspecFilename"
          '';
          checkPhase = ''
            runHook preCheck
            runHook postCheck
          '';
          installCheckPhase = ''
            runHook preInstallCheck
            make test
            runHook postInstallCheck
          '';
          meta = {
            mainProgram = "tomlua";
            maintainers = [ lib.maintainers.birdee ];
            license = lib.licenses.mit;
            homepage = "https://github.com/BirdeeHub/tomlua";
            description = "Speedy toml parsing for lua, implemented in C";
          };
        };
      # lua5_1 = prev.lua5_1.override { packageOverrides };
      l_pkg_main = builtins.mapAttrs (
        n: _: (prev.lib.attrByPath [ n "override" ] null prev) (old: {
          packageOverrides = luaself: luaprev: (if old ? packageOverrides then old.packageOverrides luaself luaprev else {}) // {
            ${APPNAME} = luaself.callPackage luaCallPackageFn {};
          };
        })
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
        ${APPNAME} = (final.neovimUtils.buildNeovimPlugin {
          luaAttr = final.neovim-unwrapped.lua.pkgs.${APPNAME};
        }).overrideAttrs (old: {
          luarocksConfig = (old.luarocksConfig or {}) // {
            lua_modules_path = "lua";
            lib_modules_path = "lua";
          };
        });
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
    checks = forAllSys (system: builtins.mapAttrs (_: p: p.overrideAttrs { doInstallCheck = true; }) packages.${system});
    devShells = forAllSys (system: let
      pkgs = getPkgs system [];
      lua = pkgs.luajit.withPackages (lp: [ lp.inspect lp.cjson lp.toml-edit lp.luarocks ]);
      default = pkgs.mkShell {
        name = "${APPNAME}-dev";
        packages = [ lua ];
        LUA_INC = "${lua}/include";
        LUA = lua.lua.pname;
        BEAR = "${pkgs.bear}/bin/bear";
        shellHook = ''
          ogdir=$(pwd)
          gitdir="$(git rev-parse --show-toplevel)"
          if [ -n "$gitdir" ]; then
            export PREFIX="$gitdir/build/test"
            cd "$gitdir"
            make clean bear
            cd "$ogdir"
          fi
          unset gitdir ogdir
        '';
      };
    in {
      inherit default;
      zsh = default.overrideAttrs (old: {
        shellHook = old.shellHook + ''
          exec zsh
        '';
      });
    });
  };
}
