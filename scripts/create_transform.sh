if [ $# != 2 ]
  then
    echo "usage ${0} Plugin ElementName"
    echo "example : ${0} streamer Streamer"
    exit
fi

SCRIPT_DIR="$(dirname -- "${BASH_SOURCE[0]}")"
source $SCRIPT_DIR/common.sh

create_element $1 $2 transform/gsttemplatetransform