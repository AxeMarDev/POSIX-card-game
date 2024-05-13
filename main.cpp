
#include <iostream>
#include <vector>
#include <pthread.h>
#include <algorithm>
#include <random>
#include <queue>
#include <deque>
#include <chrono>
#include <fstream>


// Custom barrier implementation
struct Barrier {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int crossing;

    Barrier(int num_threads) : count(num_threads), crossing(0) {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);
    }

    ~Barrier() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    void wait() {
        pthread_mutex_lock(&mutex);
        crossing++;  // One more thread through the barrier
        if (crossing == count) {
            crossing = 0;  // Reset for next use
            pthread_cond_broadcast(&cond);  // Wake up all threads
        } else {
            while (pthread_cond_wait(&cond, &mutex) != 0);
        }
        pthread_mutex_unlock(&mutex);
    }
};

// Game structure with shared resources
struct Game {
    std::queue<int> deck;
    int targetCard;
    pthread_mutex_t deckMutex;
    pthread_mutex_t printMutex;
    pthread_mutex_t updateVarMutex;
    pthread_mutex_t EvalCard;
    Barrier roundBarrier;  // Now using the custom Barrier
    bool roundWon = false;
    int playerWon = -1;
    int gaveCards = false;
    int fingavecards = false;

    bool printedRoundStart = false;
    bool printedRoundEnd = false;

    std::ofstream fileForOutput;

    Game() : roundBarrier(6) { // 6 players
        pthread_mutex_init(&deckMutex, NULL);
        pthread_mutex_init(&printMutex, NULL);
        pthread_mutex_init(&updateVarMutex, NULL);
        fileForOutput.open("output.txt");

    }

    ~Game() {
        pthread_mutex_destroy(&deckMutex);
        pthread_mutex_destroy(&printMutex);
    }
};

struct PlayerData {
    int id;
    Game* game;
   // int myCard;  // The card this player holds
    std::deque<int> cards; // will hold the 2 cards, remove from back or end

};
class Deck{
public:

    void static printDeckSync(Game * game){
        pthread_mutex_lock(&game->printMutex);

        std::queue<int> temp = game->deck;
        game->fileForOutput << "DECK: ";
        std::cout << "DECK: ";
        for(int i = 0 ; i< game->deck.size() ; i++){

            game->fileForOutput << temp.front() << ", ";
            std::cout << temp.front() << ", ";
            temp.pop();

        }
        game->fileForOutput << std::endl;
        std::cout<< std::endl;
        pthread_mutex_unlock(&game->printMutex);
    }

    void static shuffleDeckSync(Game * game, PlayerData * data,int round){
        if (data->id == round) {
            pthread_mutex_lock(&game->deckMutex);
            std::vector<int> tempVec;

            // clean queue
            std::queue<int> empty;
            std::swap( game->deck, empty );

            for (int i = 0; i < 52; ++i) {
                tempVec.push_back( i % 13 + 1);
            }

            unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
            std::default_random_engine engine(seed);

            std::shuffle(tempVec.begin(), tempVec.end(), engine );

            for( int i = 0; i < tempVec.size() ; i++){
                game->deck.push( tempVec[i]);
            }

            game->targetCard = game->deck.front();  // Set target card
            game->deck.pop();  // Remove target card from deck
            pthread_mutex_unlock(&game->deckMutex);

            pthread_mutex_lock(&game->printMutex);
            game->fileForOutput << "PLAYER " << data->id +1 << ": shuffled deck in round " << round +1 << std::endl;
            game->fileForOutput << "PLAYER " << data->id +1 << ": target card "<<game->targetCard  << std::endl;
            std::cout << "PLAYER " << data->id +1<< ": target card " << game->targetCard << std::endl;
            pthread_mutex_unlock(&game->printMutex);
        }
    }
};

class Hand{
public:
    void static printHandSync( Game * game, PlayerData * data ){
        pthread_mutex_lock(&game->printMutex);
        std::string left;
        std::string right = "_";

        if( data->cards.size() == 2){
            left = std::to_string(data->cards.front());
            right = std::to_string(data->cards.back());
            game->fileForOutput<< "PLAYER " << data->id+1 << ": hand is ( " << left << ", " << right <<" ) <> target card is " << game->targetCard << std::endl;

            std::cout << "PLAYER " << data->id +1<< ": hand is " << left << ", " << right << std::endl;
        } else {
            left = std::to_string(data->cards.front());
            game->fileForOutput << "PLAYER " << data->id+1 << ": hand is " << left  << std::endl;
        }



        pthread_mutex_unlock(&game->printMutex);
    }

    int static addToHandSync( Game * game, PlayerData * data ){
        pthread_mutex_lock(&game->deckMutex);
        data->cards.push_back(game->deck.front()) ;
        int card = game->deck.front();
        game->deck.pop();
        pthread_mutex_unlock(&game->deckMutex);

        pthread_mutex_lock(&game->printMutex);
        game->fileForOutput << "PLAYER " << data->id +1<< ": draws " << card << std::endl;
        pthread_mutex_unlock(&game->printMutex);
        return card;
    }

    int static removeFromHandSync(Game * game, PlayerData * data){

        pthread_mutex_lock(&game->deckMutex);

        std::random_device rd;
        std::mt19937 gen(rd());

        // Define a uniform distribution that covers the range [0, 1]
        std::uniform_int_distribution<> distrib(0, 1);

        int discarded;
        if ( distrib(gen) == 0){
            game->deck.push( data->cards.front() );
            discarded = data->cards.front();
            data->cards.pop_front();
        } else{
            game->deck.push( data->cards.back() );
            discarded = data->cards.back();
            data->cards.pop_back();
        }

        pthread_mutex_lock(&game->printMutex);
        game->fileForOutput << "PLAYER " << data->id +1<< ": discarded " << discarded << std::endl;
        game->fileForOutput << "PLAYER " << data->id +1<< ": hand is " << data->cards.front() << std::endl;
        pthread_mutex_unlock(&game->printMutex);

        pthread_mutex_unlock(&game->deckMutex);
    }

    bool static matchToHandsSync( Game * game, PlayerData * data ){
        if(data->cards.front() == game->targetCard || data->cards.back() == game->targetCard){
            return true;
        } else{
            return false;
        }
    }

    void static handsResetSync(PlayerData * data){
        if( !data->cards.empty()){
            data->cards.clear();
        }
    }

};




// Player thread function
void* player_thread(void* arg) {

    PlayerData* data = (PlayerData*)arg;
    Game* game = data->game;
    int roundRobin;


    for (int round = 0; round < 6; ++round) {

        pthread_mutex_lock(&game->updateVarMutex);
        if( !game->printedRoundStart ){
            game->fileForOutput << "-------------------------------------------" << std::endl;
            game->fileForOutput << "round " << round +1<< " has began" << std::endl;
            game->fileForOutput << "-------------------------------------------" << std::endl;
            game->printedRoundStart = true;
            game->printedRoundEnd = false;

        }
        pthread_mutex_unlock(&game->updateVarMutex);


        game->roundBarrier.wait(); // Wait at the barrier


        game->roundWon = false;
        game->playerWon = -1;
        game->roundBarrier.wait(); // Wait at the barrier

        // set the dealer
        Deck::shuffleDeckSync(game,data,round);

        roundRobin = round+1;
        game->roundBarrier.wait(); // Synchronize after dealer is done

        pthread_mutex_lock(&game->updateVarMutex);
        if(!game->gaveCards){
            game->fileForOutput << "PLAYER " << round +1<< ": giving each player 1 card to start " << round +1<< std::endl;
            game->gaveCards = true;
        }
        pthread_mutex_unlock(&game->updateVarMutex);

        // this will give a card to each player
        if (data->id != round && data->cards.empty()) {

            Hand::addToHandSync(game, data);
            Hand::printHandSync(game, data);

        }

        game->roundBarrier.wait(); // Wait at the barrier


        pthread_mutex_lock(&game->updateVarMutex);
        if(!game->fingavecards){
            game->fileForOutput << "PLAYER " << round +1<< ": finished giving each player 1 card to start " << round +1<< std::endl;
            game->fingavecards = true;
        }

        pthread_mutex_unlock(&game->updateVarMutex);

        game->roundBarrier.wait(); // Wait at the barrier


        // loop will continue until a winner is  found for round
        while(true){

            // will ensure round robin card evaluation
            if ( round != data->id && roundRobin % 6  == data->id){

                // will add a card to hand since there is one in between turns
                Hand::addToHandSync( game, data);
                Hand::printHandSync(game, data);

                // will compare cards and react accordingly
                pthread_mutex_lock(&game->EvalCard);
                if (  Hand::matchToHandsSync(game, data) ) {

                    pthread_mutex_lock(&game->printMutex);
                    game->fileForOutput << "PLAYER " << data->id +1<< ": won round " << round +1<< std::endl;
                    std::cout << "PLAYER " << data->id +1<< ": Won yes" << std::endl;
                    pthread_mutex_unlock(&game->printMutex);
                    game->roundWon = true;
                    game->playerWon = data->id;

                } else {
                    std::cout << "PLAYER " << data->id +1<< ": Won no" << std::endl;
                    Hand::removeFromHandSync(game, data);
                }
                Deck::printDeckSync(game);
                pthread_mutex_unlock(&game->EvalCard);

            }

            game->roundBarrier.wait(); // Synchronize end of the round

            // will evaluate and terminate round if a winner is found. Will call out losers
            if ( game->roundWon ) {
                if ( round == data->id){
                    break;
                } else if( game->playerWon != data->id){
                    pthread_mutex_lock(&game->printMutex);
                    game->fileForOutput << "PLAYER " << data->id +1 << ": lost round " << round+1  << std::endl;
                    pthread_mutex_unlock(&game->printMutex);
                }
                break;

            }

            // round robin
            roundRobin++;
            if( roundRobin + 1 % 6 == round){
                roundRobin++;
            }
            game->roundBarrier.wait();

        }

        game->roundBarrier.wait(); // Synchronize after dealer is done

        pthread_mutex_lock(&game->updateVarMutex);
        if( !game->printedRoundEnd ){
            game->fileForOutput << "-------------------------------------------" << std::endl;
            game->fileForOutput << "round " << round+1 << " has ended" << std::endl;
            game->fileForOutput << "-------------------------------------------" << std::endl;
            game->printedRoundEnd = true;
            game->printedRoundStart = false;
        }
        pthread_mutex_unlock(&game->updateVarMutex);

        game->roundBarrier.wait(); // Synchronize after dealer is done

        Hand::handsResetSync(data);

        game->gaveCards = false;
        game->fingavecards = false;

        game->roundBarrier.wait(); // Synchronize after dealer is done



    }

    return nullptr;
}

// Main function to create and manage threads
int main(int argc, char** argv) {


    // game variables
    Game game;
    pthread_t players[6];
    PlayerData playerData[6];

    // create threads
    for (int i = 0; i < 6; i++) {
        playerData[i].id = i;
        playerData[i].game = &game;
        pthread_create(&players[i], nullptr, player_thread, (void*)&playerData[i]);
    }
    // join threads
    for (auto & player : players) {
        pthread_join(player, nullptr);
    }
    game.fileForOutput << "Game ended" << std::endl;


    return 0;
}

