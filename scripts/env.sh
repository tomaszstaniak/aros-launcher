# Source from bash or zsh: . scripts/env.sh
# Precedence: environment > local.env > nothing (machine paths have no defaults).
# Relative values in local.env resolve against the directory holding it.
_ln_root=$(cd "$(dirname "${BASH_SOURCE[0]:-${(%):-%x}}")/.." && pwd)
LAUNCHER_ROOT=$_ln_root
export LAUNCHER_ROOT
if [ -f "$_ln_root/local.env" ]; then
  while IFS= read -r _line || [ -n "$_line" ]; do
    case $_line in ''|'#'*) continue ;; esac
    _key=${_line%%=*}
    _val=${_line#*=}
    _val=${_val#\"}; _val=${_val%\"}
    eval "_cur=\${$_key-}"
    if [ -z "$_cur" ] && [ -n "$_val" ]; then
      case $_val in /*) ;; *) _val="$_ln_root/$_val" ;; esac
      export "$_key=$_val"
    fi
  done < "$_ln_root/local.env"
fi
unset _ln_root _line _key _val _cur
