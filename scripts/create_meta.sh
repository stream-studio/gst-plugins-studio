if [ $# != 2 ]
  then
    echo "usage ${0} ElementName"
    echo "example : ${0} Streamer"
    exit
fi

SCRIPT_DIR="$(dirname -- "${BASH_SOURCE[0]}")"
source $SCRIPT_DIR/common.sh

create_element $1 "$2Meta" meta/gsttemplatemeta