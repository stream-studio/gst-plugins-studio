if [ $# -eq 0 ]
  then
    echo "usage ${0} plugin"
    echo "example : ${0} streamer"
    exit
fi

SCRIPT_DIR="$(dirname -- "${BASH_SOURCE[0]}")"
SRC_PATH="$SCRIPT_DIR/../src/"

export PLUGIN_NAME="$1"
export PLUGIN_DIRECTORY="$SRC_PATH$1"
export PLUGIN_C_FILE="gst$PLUGIN_NAME.c"

mkdir $PLUGIN_DIRECTORY

C_FILE_CONTENT=$(cat $SCRIPT_DIR/templates/plugin/gsttemplateplugin.c.tpl | envsubst)
echo "$C_FILE_CONTENT" > "$PLUGIN_DIRECTORY/$PLUGIN_C_FILE"

echo "
${PLUGIN_NAME}_sources = [
'$PLUGIN_NAME/$PLUGIN_C_FILE',
]

${PLUGIN_NAME} = library('${PLUGIN_NAME}',
    ${PLUGIN_NAME}_sources,
    dependencies : [gst_dep],
    c_args: plugin_c_args,
    install : true,
    install_dir : plugins_install_dir,
)" >> ${SRC_PATH}/meson.build

