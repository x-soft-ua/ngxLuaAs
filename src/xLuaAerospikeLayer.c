#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>


#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ev.h>
#include <openssl/ssl.h>

#include <aerospike/aerospike.h>
#include <aerospike/aerospike_key.h>
#include <aerospike/as_error.h>
#include <aerospike/as_key.h>
#include <aerospike/as_record.h>
#include <aerospike/as_record_iterator.h>
#include <aerospike/as_iterator.h>
#include <aerospike/as_status.h>
#include <aerospike/as_query.h>
#include <aerospike/as_list.h>
#include <aerospike/as_tls.h>
#include <aerospike/as_arraylist.h>
#include <aerospike/as_iterator.h>
#include <aerospike/as_arraylist_iterator.h>
#include <aerospike/aerospike_query.h>


#define AS_OK               0
#define AS_NEED_RECONNECT   1
#define AS_SHM_ERR          2
#define AS_SHM_CREATE_ERR   3
#define AS_SHM_TRUNCATE_ERR 4
#define AS_CLIENT_ERROR     5
#define AS_WRITE_ERR        6
#define AS_CONNECT_ERR      7
#define AS_RECORD_NOT_FOUND 8
#define AS_READ_WRITE_TIMEOUT   9

#define LSTACK_PEEK         100

#define AS_STR              1
#define AS_INT              2
#define AS_INCR             3



void std_log(const char *msg) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    fprintf(stderr, "[%d-%d-%d %d:%d:%d]  %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, msg);
}

/**
 * Get current timestamp in ms
 */
static int microtime(lua_State *L) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t currentTs = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    lua_pushnumber(L, currentTs);
    return 1;
}

/**
 * Open persistent connection to Aerospike
 *
 * @var const char * hostName
 * @var const int port
 * @var const shar * shaMemId
 */
static int private_connect_as(const char *hostName, const int port, const char* shaMemId) {
    /* ini shared memc */
    int shm = 0;
    int cStatus = 0;

    /* read pointer from shared memc */
    if ( (shm = shm_open(shaMemId, O_CREAT|O_RDWR, 0777)) == -1 ) {
        return AS_SHM_CREATE_ERR;
    }

    /* set size */
    if ( ftruncate(shm, sizeof(aerospike)) == -1 ) {
        return AS_SHM_TRUNCATE_ERR;
    }

    /* get client pointer */
    aerospike * as = (aerospike *)mmap(0, sizeof(aerospike), PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0);

    /* errors */
    as_error err;

    /* init as_client */
    as_config config;
//    config.policies.timeout = 100;
//    config.policies.write.timeout = 100;
//    config.policies.read.timeout = 50;
    as_config_init(&config);
    as_config_add_host(&config, hostName, port);

    aerospike_init(as, &config);

    /* connection */
    cStatus = aerospike_connect(as, &err);

    /* close shared memc */
    munmap(as, sizeof(aerospike));
    close(shm);
    as_error_reset(&err);

    std_log("Init aerospike connection");
    if(err.code>0 || cStatus != AEROSPIKE_OK)
    {
        std_log("Cann't establish aerospike connection");
        /* cleaning */
        munmap(as, sizeof(aerospike));
        close(shm);
        shm_unlink(shaMemId);
        return AS_CLIENT_ERROR;
    }
    return 1;
}


/**
 * Open persistent connection to Aerospike
 *
 * @var lua_State * L
 */
static int disconnect_as(lua_State *L) {

    /* shared memc ID */
    const char* shaMemId = luaL_checkstring(L, 1);

    int shm = 0;

    if ( (shm = shm_open(shaMemId, O_RDWR, 0777)) == -1 ) {
        std_log("SHM open error");
        lua_pushnumber(L, AS_SHM_ERR);
        lua_pushstring(L, "AS_SHM_ERR");
        return 2;
    }

    /* get client pointer */
    aerospike * as = (aerospike *)mmap(0, sizeof(aerospike), PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0);

    as_error err;

    /* close connection and destroy obj */
    aerospike_close(as, &err);
    aerospike_destroy(as);

    /* return to LUA result message */
    lua_pushnumber(L, err.code);
    lua_pushstring(L, err.message);

    /* cleaning */
    munmap(as, sizeof(aerospike));
    close(shm);
    shm_unlink(shaMemId);

    lua_pushnumber(L, AS_OK);
    lua_pushstring(L, "AS_OK");
    return 2;
}


/**
 * Query result struct
 */
typedef struct {
       //стек
       lua_State * L;
       int main_i;

} query_struct;

/**
 * Write bin to result struct
 *
 * @var const char * name
 * @var const as_val * value
 * @var void * tmpbuf
 */
bool print_bin2buf(const char * name, const as_val * value, void * tmpbuf) {

    /* get struct and set type */
    query_struct * this_tmp_buf = (query_struct *)tmpbuf;

    /* is number ? */
    as_integer * ivalue = as_integer_fromval(value);
    char * svalue = as_val_tostring(value);

    /* second array key */
    lua_pushstring(this_tmp_buf->L, name);

    /* write, num/str */
    if (ivalue) {
        lua_pushnumber(this_tmp_buf->L, as_integer_get(ivalue));
    } else {
        if (svalue[0] == '"'){
            memmove(&svalue[0], &svalue[1], strlen(svalue));
        }
        if (svalue[strlen(svalue)-1] == '"'){
            svalue[strlen(svalue)-1] = 0;
        }

        lua_pushstring(this_tmp_buf->L, svalue);
    }
    /* close key */
    lua_settable(this_tmp_buf->L, -3);

    /* cleaning */
    as_integer_destroy(ivalue);
    free(svalue);

    return true;
}

/**
 * Callback query method
 *
 * @var const as_val *value
 * @var void * tmpbuf
 */
bool query_proc(const as_val *value, void * tmpbuf) {
    if (value == NULL) {
        /* query is made */
        return true;
    }

    as_record *rec = as_record_fromval(value);

    if (rec != NULL) {
        /* get struct and set type */
        query_struct * this_tmp_buf = (query_struct *)tmpbuf;

        /* index */
        lua_pushnumber(this_tmp_buf->L,this_tmp_buf->main_i);
        this_tmp_buf->main_i++;

        /* second array */
        lua_newtable(this_tmp_buf->L);

        /* fill result of query */
        as_record_foreach(rec, print_bin2buf, tmpbuf);

        /* close key */
        lua_settable(this_tmp_buf->L,-3);
    }

    /* cleaning */
    as_record_destroy(rec);

    return true;
}

/**
 * Query layer
 *
 * @var lua_State *L
 */
static int query_as(lua_State *L){

    as_error err;

    const int new_execution = lua_tointeger(L, 1); /* execute new connection */
    const char* shaMemId = luaL_checkstring(L, 2); /* id in Shared Memory */
    const char *hostName = luaL_checkstring(L, 3); /* db connection hostname */
    const int port = lua_tointeger(L, 4); /* db connection port */
    const char* nameSpace = luaL_checkstring(L, 5); /* db namespace */
    const char* set = luaL_checkstring(L, 6); /* db set */
    const char* wherebin = luaL_checkstring(L, 7); /* binname (secondkey) */
    const char* wheredata = luaL_checkstring(L, 8); /* selection condition */
    const uint32_t query_timeout = lua_tointeger(L, 9); /* query timeout, ms */
    const int get_pk = lua_tointeger(L, 10); /* execute via primary key */

    /* if just run create connection */
    if(new_execution==1)
    {
        const int connect_status = private_connect_as(hostName, port, shaMemId);
        if(connect_status!=1)
        {
            lua_pushnil(L);
            lua_pushnumber(L, AS_CONNECT_ERR);
            lua_pushstring(L, "AS_CONNECT_ERR");
            return 3;
        }
    }

    int shm = 0;

    /* open shm segment */
    if ( (shm = shm_open(shaMemId, O_RDWR, 0777)) == -1 ) {
        std_log("SHM open error");
        lua_pushnil(L);
        lua_pushnumber(L, AS_SHM_ERR);
        lua_pushstring(L, "AS_SHM_ERR");
        return 3;
    }

    /* set pointer on aerospike obj */
    aerospike * as = (aerospike *)mmap(0, sizeof(aerospike), PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0);


    /* create structure and fill it */
    query_struct tmp_buf;
    tmp_buf.L = L;
    tmp_buf.main_i = 0;

    /* create LUA table */
    lua_newtable(L);


    /* primary key query ? */
    if(get_pk == 1)
    {
        as_key key;
        as_key_init(&key, nameSpace, set, wheredata);
        as_record * rec = NULL;

        /* read config, timeout */
        as_policy_read policy;
        as_policy_read_init(&policy);

        if(query_timeout)
        {
            policy.base.total_timeout = query_timeout;
        }


        as_status getStatus = aerospike_key_get(as, &err, &policy, &key, &rec);
        if ( getStatus != AEROSPIKE_OK ) {

            /* cleaning */
            as_record_destroy(rec);
            as_key_destroy(&key);

            munmap(as, sizeof(aerospike));
            close(shm);
            if (getStatus == AEROSPIKE_ERR_RECORD_NOT_FOUND) {
                //std_log("as record not found");
                lua_pushnil(L);
                lua_pushnumber(L, AS_RECORD_NOT_FOUND);
                lua_pushstring(L, "AS_RECORD_NOT_FOUND");
                return 3;
            } else if (getStatus == AEROSPIKE_ERR_QUERY_TIMEOUT ||
                       getStatus == AEROSPIKE_ERR_TIMEOUT ) {

                std_log("As r/w timeout");
                lua_pushnil(L);
                lua_pushnumber(L, AS_READ_WRITE_TIMEOUT);
                lua_pushstring(L, "AS_READ_WRITE_TIMEOUT");
                return 3;
            } else {
                std_log("As client error");
                lua_pushnil(L);
                lua_pushnumber(L, AS_CLIENT_ERROR);
                lua_pushstring(L, "AS_CLIENT_ERROR");
                return 3;
            }
        }
        else
        {
            /* index */
            lua_pushnumber(tmp_buf.L, 0);

            /* secondary array */
            lua_newtable(tmp_buf.L);

            /* fill result */
            as_record_foreach(rec, print_bin2buf, &tmp_buf);

            /* close key */
            lua_settable(tmp_buf.L,-3);

            /* cleaning */
            as_record_destroy(rec);
            as_key_destroy(&key);
        }
    }
    /* query via secondary key */
    else
    {
        as_query query;
        as_query_init(&query, nameSpace, set);

        as_query_where_init(&query, 1);
        as_query_where(&query, wherebin, as_string_equals(wheredata));

        /* query config, timeout */
        as_policy_query policy;
        as_policy_query_init(&policy);

        if(query_timeout)
        {
            policy.base.total_timeout = query_timeout;
        }

        if (aerospike_query_foreach(as, &err, &policy, &query, query_proc, &tmp_buf) !=
               AEROSPIKE_OK) {

            /* cleaning */
            as_query_destroy(&query);

            munmap(as, sizeof(aerospike));
            close(shm);
            lua_pushnil(L);
            lua_pushnumber(L, AS_NEED_RECONNECT);
            lua_pushstring(L, "AS_NEED_RECONNECT");
            return 3;
        }

        /* cleaning */
        as_query_destroy(&query);
    }

    //lua_pushlightuserdata(L, as);

    munmap(as, sizeof(aerospike));
    close(shm);


    lua_pushnumber(L, AS_OK);
    lua_pushstring(L, "AS_OK");
    return 3;
}

/**
 * Put bin into record
 *
 * @var lua_State *L
 */
static int put_bin(lua_State *L){

    as_error err;

    const int new_execution = lua_tointeger(L, 1); /* execute new connection */
    const char* shaMemId = luaL_checkstring(L, 2); /* id in Shared Memory */
    const char *hostName = luaL_checkstring(L, 3); /* db connection hostname */
    const int port = lua_tointeger(L, 4); /* db connection port */
    const char* nameSpace = luaL_checkstring(L, 5); /* db namespace */
    const char* set = luaL_checkstring(L, 6); /* db set */
    const char* binname = luaL_checkstring(L, 7); /* binname */
    const char* pk = luaL_checkstring(L, 8); /* primary key */
    const char* binval_str = luaL_checkstring(L, 9); /* binval */
    const int binval_int = lua_tointeger(L, 10);
    const int operation = lua_tointeger(L, 11); /* operation */
    const int ttl = lua_tointeger(L, 12); /* ttl */
    const uint32_t query_timeout = lua_tointeger(L, 9); /* query timeout, ms */


    /* if just run create connection */
    if(new_execution==1)
    {
        const int connect_status = private_connect_as(hostName, port, shaMemId);
        if(connect_status!=1)
        {
            lua_pushnil(L);
            lua_pushnumber(L, AS_CONNECT_ERR);
            lua_pushstring(L, "AS_CONNECT_ERR");
            return 3;
        }
    }


    int shm = 0;

    /* open shm segment */
    if ( (shm = shm_open(shaMemId, O_RDWR, 0777)) == -1 ) {
        std_log("SHM open error");
        lua_pushnil(L);
        lua_pushnumber(L, AS_SHM_ERR);
        lua_pushstring(L, "AS_SHM_ERR");
        return 3;
    }

    /* set pointer on aerospike obj */
    aerospike * as = (aerospike *)mmap(0, sizeof(aerospike), PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0);


    as_record rec;
    as_record_inita(&rec, 2);

    rec.ttl = ttl;

    as_key key;
    as_key_init(&key, nameSpace, set, pk);

    if (operation==AS_STR) {
        as_policy_write policy;
        as_policy_write_init(&policy);

        if(query_timeout)
        {
            policy.base.total_timeout = query_timeout;
        }
        as_record_set_str(&rec, binname, binval_str);
        if (aerospike_key_put(as, &err, &policy, &key, &rec) != AEROSPIKE_OK) {
            std_log("as write error");
            lua_pushnil(L);
            lua_pushnumber(L, AS_WRITE_ERR);
            lua_pushstring(L, "AS_WRITE_ERR");
            return 3;
        }
    }
    else if(operation==AS_INT)
    {
        as_policy_write policy;
        as_policy_write_init(&policy);

        if(query_timeout)
        {
            policy.base.total_timeout = query_timeout;
        }
        as_record_set_int64(&rec, binname, binval_int);
        if (aerospike_key_put(as, &err, &policy, &key, &rec) != AEROSPIKE_OK) {
            std_log("as write error");
            lua_pushnil(L);
            lua_pushnumber(L, AS_WRITE_ERR);
            lua_pushstring(L, "AS_WRITE_ERR");
            return 3;
        }
    }
    else if(operation==AS_INCR)
    {
        as_operations ops;
        as_operations_inita(&ops, 2);
        as_operations_add_incr(&ops, binname, binval_int);
        as_operations_add_read(&ops, binname);

        as_record * rec_ = &rec;

        as_policy_operate policy;
        as_policy_operate_init(&policy);

        if(query_timeout)
        {
            policy.base.total_timeout = query_timeout;
        }

        if (aerospike_key_operate(as, &err, &policy, &key, &ops, &rec_) != AEROSPIKE_OK) {
            std_log("as write error");
            lua_pushnil(L);
            lua_pushnumber(L, AS_WRITE_ERR);
            lua_pushstring(L, "AS_WRITE_ERR");
            return 3;
        }

        as_integer * ivalue = as_record_get_integer(&rec, binname);
        lua_pushnumber(L, as_integer_get(ivalue));
        as_integer_destroy(ivalue);

        /* cleaning */
        as_operations_destroy(&ops);
    }


    /* cleaning */
    as_record_destroy(&rec);
    as_key_destroy(&key);

    /* unmap shm */
    munmap(as, sizeof(aerospike));
    close(shm);

    /* return result */
    lua_pushnumber(L, AS_OK);
    lua_pushstring(L, "AS_OK");
    return 3;
}


/**
 * Bind methods struct
 */
static const struct luaL_Reg as_client [] = {
		{"query_as", query_as},
		{"disconnect_as", disconnect_as},
		{"put_bin", put_bin},
		{"microtime", microtime},
		{NULL, NULL}
};

/**
 * Execute binding
 */
extern int luaopen_xLuaAerospikeLayer(lua_State *L){
	luaL_register(L, "xLuaAerospikeLayer", as_client);
	return 0;
}
