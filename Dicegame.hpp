#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/transaction.hpp>

#define EOS_SYMBOL symbol("EOS",4)

using namespace eosio;
using namespace std;
using eosio::unpack_action_data;

class [[eosio::contract]] Dicegame : public eosio::contract {

public:

    const uint32_t TWO_MINUTES = 2 * 60;
    const uint64_t MIN_BET = 1000;
    const uint64_t HOUSEEDGE_TIMES10000 = 200;
    const uint64_t HOUSEEDGE_REF_TIMES10000 = 150;
    const uint64_t REFERRER_REWARD_TIMES10000 = 50;

    Dicegame( name receiver, name code, datastream<const char*>ds ):contract( receiver, code, ds ),
        _partake( receiver, receiver.value ),
        _betusers( receiver, receiver.value),
        _betgames( receiver, receiver.value )
    {}

    [[eosio::action]]
    void addpartake( name username );
    
    void transfer( uint64_t sender, uint64_t receiver );

    [[eosio::action]]
    void betresolve( const name username, capi_signature signature );

    [[eosio::action]]
    void betreceipt( name accountName, name contractName, asset betAsset, asset payoutAsset, 
    string result, uint64_t rollNumber, uint64_t randomRoll, string hashSeed, capi_checksum256 hashSeedHash, signature signature );

    [[eosio::action]]
    void rmpartake( name username );

    [[eosio::action]]
    void cancelbet( const uint64_t gameId, string message );

    [[eosio::action]]
    void refundbet( const uint64_t gameId, string message );


private:

    struct [[eosio::table]] partakeadd {
        
        name username;

        auto primary_key() const { return username.value; }
    };

    struct [[eosio::table]] betuser {

        name username;
        capi_checksum256 hash;

        uint64_t primary_key() const { return username.value; }

        EOSLIB_SERIALIZE( betuser, (username)(hash) );
    };

    struct [[eosio::table]] betgame {
        
        uint64_t betId;
        name accountName;
        name contractName;
        asset betAsset;
        asset payoutAsset;
        uint8_t result;
        uint64_t roll;
        uint64_t randomRoll;
        string hashSeed;
        capi_checksum256 hashSeedHash;
        capi_signature signature;
        time_point_sec betTime;

        uint64_t primary_key() const { return accountName.value; }

        EOSLIB_SERIALIZE( betgame, (betId)(accountName)(contractName)(betAsset)(payoutAsset)(result)(roll)(randomRoll)(hashSeed)(hashSeedHash)(signature)(betTime) );

    };

    struct transfer_args {
        name from;
        name to;
        asset quantity;
        std::string memo;
    };
    
    struct account {
        asset    balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    

    typedef eosio::multi_index< name("referpartake"), partakeadd > partake_index;
    typedef eosio::multi_index< name("betusers"), betuser > user_index;     
    typedef eosio::multi_index< name("betgames"), betgame > betresolveindex;
    typedef eosio::multi_index< name("accounts"), account > accounts;

    partake_index _partake;
    betresolveindex _betgames;
    user_index _betusers;

    uint64_t get_payout_mult_times10000( const uint64_t roll_under, const uint64_t house_edge_times_10000 ) const {

        return ( (10000 - house_edge_times_10000) * 100 ) / ( roll_under - 1 );
    }

    uint64_t get_max_win( const uint64_t betAmount, const uint64_t rollUnder ) {

        return ( ( betAmount * get_payout_mult_times10000( rollUnder, REFERRER_REWARD_TIMES10000 ) ) / 10000 ) - betAmount;
    }

    string to_hex( const char* d, uint32_t s ) {

        std::string r;
        const char* to_hex = "0123456789abcdef";
        uint8_t* c  = (uint8_t*)d;
        for ( uint32_t i = 0; i < s; ++i ) {
            ( r += to_hex[(c[i] >> 4)] ) += to_hex[(c[i] & 0x0f )];
        }
        
        return r;
    }

    string sha256_to_hex( const capi_checksum256& sha256 ) {

        return to_hex( (char*)sha256.hash, sizeof(sha256.hash) );
    }

};
