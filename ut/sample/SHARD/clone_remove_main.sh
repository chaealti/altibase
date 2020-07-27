#!/usr/bin/env bash

# �Ʒ��� ��Ȳ�� �����Ѵ�.
#     - Target Node : Node3
#     - All Active(Standby) Node : Node1(NodeA), Node2(NodeB), Node3(NodeC)
#     - Clone ���� ��ü : T2(Node1, NodeA, Node2, NodeB, Node3, NodeC)
#
# T2�� Node3(NodeC)���� �����Ѵ�.
#     T2(Node1, NodeA, Node2, NodeB, Node3, NodeC)
#     ->
#     T2(Node1, NodeA, Node2, NodeB)

DIR_PATH=$( cd `dirname $0` && pwd )
source ${DIR_PATH}/configure.sh

IFS_BACKUP=${IFS}

ISQL="${ALTIBASE_HOME}/bin/isql -u SYS -p ${PASSWORD} -silent"

# ��� Node�� ���� ������ ��´�.
META_SQL_RESULT=$(
${ISQL} -s ${SHARD_META_SERVER} -port ${SHARD_META_PORT} << EOF
    SET HEADING OFF;
    EXEC utl_shard_online_rebuild.get_connection_info();
    QUIT
EOF
)

DIR_PATH=$(cd `dirname $0` && pwd)
/usr/bin/env bash ${DIR_PATH}/clone_remove_check_main.sh ${1}
if [ $? = 1 ]; then
    echo "${APPLICATION} can not be executed"
    exit 1
fi

# ��� Node�� ������ Data SMN�� �����Ѵ�. (�� Node�� ����)
echo ""
echo "[Step-1. (All nodes) Set new data SMN]"

IFS=$'\n'
RESULT_ARRAY=$( echo "${META_SQL_RESULT}" | grep -v "^\[NODE_NAME\]" | grep -v "^Execute success." )
for VALUE in ${RESULT_ARRAY[@]}
do
    NODE_NAME=$( echo ${VALUE} | gawk '{print $1}' )
    NODE_IP=$( echo ${VALUE} | gawk '{print $2}' )
    NODE_PORT=$( echo ${VALUE} | gawk '{print $3}' )
    NODE_ALTERNATE_IP=$( echo ${VALUE} | gawk '{print $4}' )
    NODE_ALTERNATE_PORT=$( echo ${VALUE} | gawk '{print $5}' )

    IFS=$' '

    echo "Node Info. : ${NODE_NAME}, ${NODE_IP}, ${NODE_PORT}"

    ${ISQL} -s ${NODE_IP} -port ${NODE_PORT} << EOF
        ALTER SYSTEM RELOAD SHARD META NUMBER LOCAL;
        QUIT
EOF

    if [ ${NODE_ALTERNATE_IP} != "N/A" ]
    then
        echo "Alternate Node Info. : ${NODE_NAME}, ${NODE_ALTERNATE_IP}, ${NODE_ALTERNATE_PORT}"

        ${ISQL} -s ${NODE_ALTERNATE_IP} -port ${NODE_ALTERNATE_PORT} << EOF
            ALTER SYSTEM RELOAD SHARD META NUMBER LOCAL;
            QUIT
EOF
    fi
done

IFS=${IFS_BACKUP}

echo "Done."
