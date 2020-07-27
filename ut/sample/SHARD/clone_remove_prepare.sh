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

# FROM_NODE_NAME�� �����ߴ��� �˻��Ѵ�.
if [ ! ${FROM_NODE_NAME} ]
then
    echo "FROM_NODE_NAME is needed."
    exit 1
fi

DIR_PATH=$(cd `dirname $0` && pwd)
/usr/bin/env bash ${DIR_PATH}/clone_remove_check_prepare.sh ${1}
if [ $? = 1 ]; then
    echo "${APPLICATION} can not be executed"
    exit 1
fi

# Reset Shard Resident Node SQL�� �����Ѵ�.
SHARD_META_SQL=""

for (( i = 0 ; i < $SHARD_OBJECTS_COUNT ; i++ ))
do
    SHARD_META_SQL="${SHARD_META_SQL}
        EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE( '${USER_NAME_ARRAY[$i]}',
                                                   '${TABLE_NAME_ARRAY[$i]}',
                                                   '${FROM_NODE_NAME}',
                                                   NULL );"
done

echo ""
echo "Shard Meta SQL : ${SHARD_META_SQL}"

# ��� Node�� ���� ������ ��´�.
META_SQL_RESULT=$(
${ISQL} -s ${SHARD_META_SERVER} -port ${SHARD_META_PORT} << EOF
    SET HEADING OFF;
    EXEC utl_shard_online_rebuild.get_connection_info();
    QUIT
EOF
)

# ��� Node�� Shard Meta���� Clone ���� ��ü�� Node�� �����Ѵ�.
echo ""
echo "[Step-1. (All nodes) Unset Shard Clone]"

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

    # T2�� Node���� Node3(NodeC)�� �����Ѵ�.
    echo "Node Info. : ${NODE_NAME}, ${NODE_IP}, ${NODE_PORT}"

    ${ISQL} -s ${NODE_IP} -port ${NODE_PORT} << EOF
        ALTER SESSION SET AUTOCOMMIT = FALSE;
        ${SHARD_META_SQL}
        COMMIT;
        QUIT
EOF

    if [ ${NODE_ALTERNATE_IP} != "N/A" ]
    then
        echo "Alternate Node Info. : ${NODE_NAME}, ${NODE_ALTERNATE_IP}, ${NODE_ALTERNATE_PORT}"

        ${ISQL} -s ${NODE_ALTERNATE_IP} -port ${NODE_ALTERNATE_PORT} << EOF
            ALTER SESSION SET AUTOCOMMIT = FALSE;
            ${SHARD_META_SQL}
            COMMIT;
            QUIT
EOF
    fi
done

IFS=${IFS_BACKUP}

echo "Done."
