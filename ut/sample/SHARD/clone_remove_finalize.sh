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
/usr/bin/env bash ${DIR_PATH}/clone_remove_check_finalize.sh ${1}
if [ $? = 1 ]; then
    echo "${APPLICATION} can not be executed"
    exit 1
fi

# From Node ���� ������ �����Ѵ�.
IFS=$'\n'
RESULT_ARRAY=$( echo "${META_SQL_RESULT}" | grep -v "^\[NODE_NAME\]" | grep -v "^Execute success." )
for VALUE in ${RESULT_ARRAY[@]}
do
    NODE_NAME=$( echo ${VALUE} | gawk '{print $1}' )
    NODE_IP=$( echo ${VALUE} | gawk '{print $2}' )
    NODE_PORT=$( echo ${VALUE} | gawk '{print $3}' )
    NODE_ALTERNATE_IP=$( echo ${VALUE} | gawk '{print $4}' )
    NODE_ALTERNATE_PORT=$( echo ${VALUE} | gawk '{print $5}' )

    if [ ${NODE_NAME} = ${FROM_NODE_NAME} ]
    then
        FROM_NODE_IP=${NODE_IP}
        FROM_NODE_PORT=${NODE_PORT}
        if [ ${NODE_ALTERNATE_IP} != "N/A" ]
        then
            FROM_NODE_ALTERNATE_IP=${NODE_ALTERNATE_IP}
            FROM_NODE_ALTERNATE_PORT=${NODE_ALTERNATE_PORT}
        fi
    fi
done

IFS=${IFS_BACKUP}

ISQL_FROM_NODE="${ISQL} -s ${FROM_NODE_IP} -port ${FROM_NODE_PORT}"
if [ ${FROM_NODE_ALTERNATE_IP} ]
then
    ISQL_FROM_NODE_ALTERNATE="${ISQL} -s ${FROM_NODE_ALTERNATE_IP} -port ${FROM_NODE_ALTERNATE_PORT}"
fi

# Node3(NodeC)���� Clone ���� ��ü�� �����͸� �����Ѵ�.
#   Node3���� T2�� Truncate�Ѵ�.
echo ""
echo "[Step-1. (Target nodes) Truncate Table]"
echo "Node Info. : ${FROM_NODE_NAME}, ${FROM_NODE_IP}, ${FROM_NODE_PORT}"

for (( i = 0 ; i < $SHARD_OBJECTS_COUNT ; i++ ))
do
    TRUNCATE_TABLE_SQL="TRUNCATE TABLE \"${USER_NAME_ARRAY[$i]}\".\"${TABLE_NAME_ARRAY[$i]}\""
    echo "DDL : ${TRUNCATE_TABLE_SQL}"

    ${ISQL_FROM_NODE} << EOF
        ${TRUNCATE_TABLE_SQL};
        QUIT
EOF
done

if [ ${FROM_NODE_ALTERNATE_IP} ]
then
    #   [Check] (Node3 -> NodeC) Shard Data Replication Flush
    if [ ${REP_NAME_WITH_ALTERNATE_IN_FROM_NODE} ]
    then
        ${ISQL_FROM_NODE} << EOF
            EXEC utl_shard_online_rebuild.flush_replication( '${REP_NAME_WITH_ALTERNATE_IN_FROM_NODE}' );
            QUIT
EOF
    fi

    #   NodeC���� T2�� Truncate�Ѵ�.
    echo "Alternate Node Info. : ${FROM_NODE_NAME}, ${FROM_NODE_ALTERNATE_IP}, ${FROM_NODE_ALTERNATE_PORT}"

    for (( i = 0 ; i < $SHARD_OBJECTS_COUNT ; i++ ))
    do
        TRUNCATE_TABLE_SQL="TRUNCATE TABLE \"${USER_NAME_ARRAY[$i]}\".\"${TABLE_NAME_ARRAY[$i]}\""
        echo "DDL : ${TRUNCATE_TABLE_SQL}"

        ${ISQL_FROM_NODE_ALTERNATE} << EOF
            ${TRUNCATE_TABLE_SQL};
            QUIT
EOF
    done
fi

echo "Done."
