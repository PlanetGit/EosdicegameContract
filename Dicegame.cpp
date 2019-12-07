#include "Dicegame.hpp"

void Dicegame::addpartake( name username ) {

    require_auth( name(username) );

    capi_checksum256 hashValue;
    sha256( (char*)&username, username.length(), &hashValue );

    auto user_iterator = _betusers.find( username.value );
    if ( user_iterator == _betusers.end() ) {

        user_iterator = _betusers.emplace( username, [&]( auto& user ){
            
            user.username = username;
            user.hash = hashValue;
        });
    }
}


void Dicegame::rmpartake( name username ) {

    require_auth( name( username ) );
    auto users_itr = _betusers.find( username.value );
    _betusers.erase( users_itr );
}

void Dicegame::betresolve( const name username, capi_signature signature ) {

    auto bet_iterator = _betgames.find( username.value );
    eosio_assert( bet_iterator != _betgames.end(), "Bet doesn't exist" );

    capi_checksum256 random_num_hash;
	sha256( (char *)&signature, sizeof(signature), &random_num_hash );

    const uint64_t random_roll = ((random_num_hash.hash[0] + random_num_hash.hash[1] + random_num_hash.hash[2] + random_num_hash.hash[3] + random_num_hash.hash[4] + random_num_hash.hash[5] + random_num_hash.hash[6] + random_num_hash.hash[7]) % 100) + 1;

    uint64_t edge = HOUSEEDGE_TIMES10000;
    uint64_t ref_reward = 0;
    uint64_t payout = 0;
    uint8_t game = 0;

    if ( random_roll < bet_iterator->roll ) {
        payout = ( bet_iterator->betAsset.amount * get_payout_mult_times10000(bet_iterator->roll, edge) ) / 10000;
    }

    if ( payout > 0 ) {
        
        game = 1;
        action counter = action( 
            permission_level{ _self, "active"_n }, 
            "eosio.token"_n,
            "transfer"_n,
            std::make_tuple( 
                _self,
                bet_iterator->accountName,
                asset( payout, EOS_SYMBOL ),
                string( bet_iterator->accountName.to_string() ) + std::string( " -- Winner! Play: numeris.one" )
            )
        );

        counter.send();
    }

    _betgames.modify( bet_iterator, _self, [&]( auto& betmod ){
            
        betmod.contractName = "eosio.token"_n;
        betmod.payoutAsset = asset( payout, EOS_SYMBOL );
        betmod.result = game;
        betmod.randomRoll = random_roll;
        betmod.signature = signature;
    });

}

void Dicegame::betreceipt( name accountName, name contractName, asset betAsset, asset payoutAsset, 
    string result, uint64_t rollNumber, uint64_t randomRoll, string hashSeed, capi_checksum256 hashSeedHash, signature signature ) {

    require_auth( name(accountName) );
    auto bet_iterator = _betgames.find( accountName.value );
    
    require_recipient( accountName ); 
    
    _betgames.erase( bet_iterator );
}

void Dicegame::transfer( uint64_t sender, uint64_t receiver ) {

    string roll_str, roll_strtmp, ref_str, seed_str, seed_strtmp;
    capi_checksum256 user_seed_hash, tx_hash, seed_hash;

    auto transfer_data = unpack_action_data<transfer_args>();
    if ( transfer_data.from == _self || transfer_data.from == "Dicegame"_n ){
        return;
    }
    
    eosio_assert( transfer_data.quantity.is_valid(), "Invalid asset");
    const uint64_t your_bet_amount = (uint64_t)transfer_data.quantity.amount;
    const asset bet_amount = asset( your_bet_amount, EOS_SYMBOL );

    size_t ftmp = transfer_data.memo.find( "," );
    const size_t first_break = ftmp;

    if ( first_break != string::npos ) {
        
        roll_strtmp = transfer_data.memo.substr( 0, first_break );
        size_t stmp = roll_strtmp.find( ":" );
        const size_t second_break = stmp + 1;
        roll_str = roll_strtmp.substr( second_break );

        const size_t firstplus = ftmp + 1;
        seed_strtmp = transfer_data.memo.substr( firstplus );
        size_t thtmp = seed_strtmp.find( ":" );
        const size_t third_break = thtmp + 1;
        seed_str = seed_strtmp.substr( third_break );
    } else {

        roll_str = "";
        seed_str = "";
    }

    const uint64_t roll_under = stoull(roll_str, 0, 10);
    eosio_assert( roll_under >= 2 && roll_under <= 96, "Roll must be >= 2, <= 96.");
    const uint64_t your_win_amount = ( your_bet_amount * get_payout_mult_times10000( roll_under, HOUSEEDGE_REF_TIMES10000 ) / 10000 ) - your_bet_amount;
    eosio_assert( your_win_amount <= get_max_win( your_bet_amount, roll_under ), "Bet less than max" );

    sha256( (char *)&seed_str, sizeof( seed_str ) * 2, &user_seed_hash );
    auto s = read_transaction(nullptr, 0);
    char *tx = (char *)malloc(s);
    read_transaction(tx, s);
    sha256( tx, s, &tx_hash );

    string seed = sha256_to_hex( user_seed_hash );
    string seeds = seed_str + string( ":" ) + seed + string( ":" ) + string( "eosio.token:" ) + bet_amount.to_string() + string( ":" ) + string( roll_str );
    sha256( (char *)&seeds, sizeof( seeds ) * 2, &seed_hash );

    const uint64_t bet_id = ((uint64_t)tx_hash.hash[0] << 32) + ((uint64_t)tx_hash.hash[1] << 16) + ((uint64_t)tx_hash.hash[2] << 8) + ((uint64_t)tx_hash.hash[3] << 4) + ((uint64_t)tx_hash.hash[4] << 2) + ((uint64_t)tx_hash.hash[5]);


    _betgames.emplace( _self, [&]( auto& bet ){
        
        bet.betId = bet_id;
        bet.accountName = transfer_data.from;
        bet.betAsset = bet_amount;
        bet.roll = roll_under;
        bet.hashSeed = seeds;
        bet.hashSeedHash = seed_hash;
        bet.betTime = time_point_sec(now());
    });
}

void Dicegame::cancelbet( uint64_t gameId, string message ){
    
    require_auth( "Dicegame"_n );

    auto betgames_itr = _betgames.find(gameId);
    eosio_assert( betgames_itr != _betgames.end(), "No bet exists" );

    const auto bettor_name = betgames_itr->accountName;
    string bettor_str = bettor_name.to_string();

    action counter = action( 
        permission_level{get_self(), "active"_n}, 
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple( 
            _self,
            "numeristest1"_n,                    
            betgames_itr->betAsset,
            bettor_str + string( ":" ) + message
        )
    );

    counter.send();

    _betgames.erase( betgames_itr );
}


void Dicegame::refundbet( const uint64_t gameId, string message ) {

    require_auth( "Dicegame"_n );

    auto betgames_itr = _betgames.find( gameId );
    eosio_assert( betgames_itr != _betgames.end(), "Game doesn't exist" );

    const time_point_sec bet_time = betgames_itr->betTime;
    eosio_assert( time_point_sec(now() - TWO_MINUTES) > bet_time, "Wait 10 minutes" );

    const auto n = betgames_itr->accountName;
    string str = n.to_string();

    action counter = action( 
        permission_level{ _self, "active"_n }, 
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple( 
            _self,
            "numeristest1"_n,
            betgames_itr->betAsset,
            std::string( str ) + std::string( "Bet id: " ) + std::to_string( gameId ) + std::string( " -- REFUND. Sorry for the inconvenience." )
        )
    );

    counter.send();
    _betgames.erase(betgames_itr);
}

#define EOSIO_DISPATCH_CM( TYPE, MEMBERS ) \
extern "C" { \
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
        if( code == receiver || code == name("eosio.token").value ) { \
            if (action == name("transfer").value) {\
                eosio_assert( code == name("eosio.token").value, "Must transfer EOS");\
            }\
            switch( action ) { \
                EOSIO_DISPATCH_HELPER( TYPE, MEMBERS ) \
            } \
            /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
        } \
   } \
} \

EOSIO_DISPATCH_CM( Dicegame, (addpartake)(betresolve)(rmpartake)(betreceipt)(transfer)(cancelbet)(refundbet) )