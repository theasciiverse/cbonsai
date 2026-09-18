# cbonsai(1) completion

_cbonsai()
{
  local cur prev opts
  _get_comp_words_by_ref cur prev

  opts=(
    '-l'
    '--live'
    '-t'
    '--time'
    '-i'
    '--infinite'
    '-w'
    '--wait'
    '-S'
    '--screensaver'
    '-m'
    '--message'
    '-T'
    '--text-origin'
    '-b'
    '--base'
    '-g'
    '--geometry'
    '-c'
    '--leaf'
    '-k'
    '--colors'
    '-M'
    '--multiplier'
    '-L'
    '--life'
    '-p'
    '--print'
    '-s'
    '--seed'
    '-W'
    '--save'
    '-C'
    '--load'
    '-v'
    '--verbose'
    '-h'
    '--help'
  )

  case "$prev" in
    -[WC]|--save|--load)
      COMPREPLY=($(compgen -f -- "$cur"))
      return
      ;;
    -[twmbTbgckMLs]|--time|--wait|--message|--text-origin|--base|--geometry|--leaf|--colors|--multiplier|--life|--seed)
      return
      ;;
  esac

  COMPREPLY=($(compgen -W "${opts[*]}" -- "$cur"))

} &&
complete -F _cbonsai cbonsai
