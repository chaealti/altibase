#!/usr/bin/env bash

# �Ʒ��� ��Ȳ�� �����Ѵ�.
#     - All Active(Standby) Node : Node1(NodeA), Node2(NodeB), Node3(NodeC)
#     - Hash, Range, List ���� ��ü : T1 (Partition : P1, P2, P3)
#     - Clone ���� ��ü : T2
#     - Solo ���� ��ü : T3
#     - Data Rebuild�� �����Ͽ� �л� �������� Node3(NodeC)�� ���� ��ü�� ������ �����̴�.
#
# Node3(NodeC)�� �����Ѵ�.

APPLICATION=$0

if [ $# -ne 4 ]
then
    echo "Usage : ${APPLICATION} shard_meta_server shard_meta_port sys_password old_node_name"
    exit 1
fi

SHARD_META_SERVER=$1
SHARD_META_PORT=$2
PASSWORD=$3
OLD_NODE_NAME=$4

IFS_BACKUP=${IFS}

ISQL="${ALTIBASE_HOME}/bin/isql -u SYS -p ${PASSWORD} -silent"

# ��� Node�� ���� ������ ��´�. (�߰� Node ����)
META_SQL_RESULT=$(
${ISQL} -s ${SHARD_META_SERVER} -port ${SHARD_META_PORT} << EOF
    SET HEADING OFF;
    EXEC utl_shard_online_rebuild.get_connection_info();
    QUIT
EOF
)

DIR_PATH=$(cd `dirname $0` && pwd)
/usr/bin/env bash ${DIR_PATH}/node_remove_check_prepare.sh ${SHARD_META_SERVER} ${SHARD_META_PORT} ${PASSWORD} ${OLD_NODE_NAME}
if [ $? = 1 ]; then
    echo "${APPLICATION} can not be executed"
    exit 1
fi

# ��� Node�� Shard Meta���� Node3(NodeC)�� �����Ѵ�. (DBMS_SHARD.UNSET_NODE())
echo "[Step-1. (All nodes) Unset Node]"

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
        EXEC DBMS_SHARD.UNSET_NODE( '${OLD_NODE_NAME}' );
        QUIT
EOF

    if [ ${NODE_ALTERNATE_IP} != "N/A" ]
    then
        echo "Alternate Node Info. : ${NODE_NAME}, ${NODE_ALTERNATE_IP}, ${NODE_ALTERNATE_PORT}"

        ${ISQL} -s ${NODE_ALTERNATE_IP} -port ${NODE_ALTERNATE_PORT} << EOF
            EXEC DBMS_SHARD.UNSET_NODE( '${OLD_NODE_NAME}' );
            QUIT
EOF
    fi
done

IFS=${IFS_BACKUP}

echo "Done."
