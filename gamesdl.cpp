#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <algorithm>
#include <fstream>

using namespace std;

const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 800;

enum GameState {
    MENU, PLAYING, GAME_OVER, HOW_TO_PLAY, SETTINGS, HIGHSCORE
};

struct Button {
    SDL_Rect rect;
    string text;

    Button(int x, int y, int w, int h, const string& t) : rect{x, y, w, h}, text(t) {}

    bool isMouseOver(int x, int y) {
        return (x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h);
    }

    void render(SDL_Renderer* renderer, TTF_Font* font) {
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 200);
        SDL_RenderFillRect(renderer, &rect);

        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &rect);

        SDL_Color white = {255, 255, 255};
        SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), white);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_Rect textRect = {
            rect.x + (rect.w - surface->w) / 2,
            rect.y + (rect.h - surface->h) / 2,
            surface->w,
            surface->h
        };

        SDL_RenderCopy(renderer, texture, NULL, &textRect);

        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
};

struct Bullet {
    SDL_Rect rect;
    float vx, vy;
    int texIndex;

    Bullet(int x, int y, float vx, float vy, int texIndex) {
        rect = {x, y, 30, 30};
        this->vx = vx;
        this->vy = vy;
        this->texIndex = texIndex;
    }

    void move() {
        rect.x += vx;
        rect.y += vy;
    }

    void render(SDL_Renderer* renderer, SDL_Texture* bulletTextures[6]) {
        SDL_RenderCopy(renderer, bulletTextures[texIndex], NULL, &rect);
    }
};

struct Player {
    SDL_Rect rect;
    int speed = 4;
    SDL_Texture* tex = nullptr;

    void moveTo(int targetX, int targetY) {
        float dx = targetX - rect.x;
        float dy = targetY - rect.y;
        float dist = sqrt(dx * dx + dy * dy);
        if (dist > speed) {
            rect.x += (dx / dist) * speed;
            rect.y += (dy / dist) * speed;
        }
    }

    void render(SDL_Renderer* renderer) {
        SDL_RenderCopy(renderer, tex, NULL, &rect);
    }
};

struct Shield {
    SDL_Rect rect;
    Uint32 spawnTime;

    Shield(int x, int y, bool horizontal) {
        if (horizontal)
            rect = {x - 50, y - 10, 100, 20};
        else
            rect = {x - 10, y - 50, 20, 100};
        spawnTime = SDL_GetTicks();
    }

    bool isExpired() {
        return SDL_GetTicks() - spawnTime > 2000;
    }

    void render(SDL_Renderer* renderer, SDL_Texture* tex) {
        SDL_RenderCopy(renderer, tex, NULL, &rect);
    }
};

struct ScoreEntry {
    string name;
    int score;

    ScoreEntry(string n, int s) : name(n), score(s) {}
};

bool checkCollision(SDL_Rect a, SDL_Rect b) {
    return SDL_HasIntersection(&a, &b);
}

string intToString(int n) {
    return to_string(n);
}

vector<ScoreEntry> loadHighScores() {
    vector<ScoreEntry> scores;
    ifstream inFile("highscores.txt");
    string name;
    int score;

    if (inFile.is_open()) {
        while (inFile >> name >> score) {
            scores.push_back(ScoreEntry(name, score));
        }
        inFile.close();
    }

    if (scores.empty()) {
        scores.push_back(ScoreEntry("Player1", 0));
        scores.push_back(ScoreEntry("Player2", 0));
        scores.push_back(ScoreEntry("Player3", 0));
        scores.push_back(ScoreEntry("Player4", 0));
        scores.push_back(ScoreEntry("Player5", 0));
    }

    return scores;
}

void saveHighScores(const vector<ScoreEntry>& scores) {
    ofstream outFile("highscores.txt");
    if (outFile.is_open()) {
        for (const auto& entry : scores) {
            outFile << entry.name << " " << entry.score << endl;
        }
        outFile.close();
    }
}

int getHighestScore() {
    vector<ScoreEntry> scores = loadHighScores();
    int highest = 0;
    for (const auto& entry : scores) {
        highest = max(highest, entry.score);
    }
    return highest;
}

void updateHighScores(int newScore) {
    vector<ScoreEntry> scores = loadHighScores();
    bool updated = false;
    for (auto& score : scores) {
        if (newScore > score.score) {
            score.name = "Player";
            score.score = newScore;
            updated = true;
            break;
        }
    }

    if (updated) {
        sort(scores.begin(), scores.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
            return a.score > b.score;
        });
        saveHighScores(scores);
    }
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, const string& text, int x, int y, SDL_Color color = {255, 255, 255}) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void resetGame(Player& player, vector<Bullet>& bullets, vector<Shield>& shields, int& survivalTime, Uint32& lastWall,
                bool& firstShieldUsed, Uint32& startTime, int& nextBulletTypeToSpawn, int& targetX, int& targetY,
                int& currentMouseX, int& currentMouseY, SDL_Texture* playerTexRight) {
    bullets.clear();
    shields.clear();
    player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
    player.tex = playerTexRight;
    survivalTime = 0;
    lastWall = 0;
    firstShieldUsed = false;
    startTime = SDL_GetTicks();
    nextBulletTypeToSpawn = 0;
    targetX = player.rect.x;
    targetY = player.rect.y;
    currentMouseX = targetX;
    currentMouseY = targetY;
}

void spawnBullet(vector<Bullet>& bullets, int playerX, int playerY, int& nextBulletTypeToSpawn) {
    int side = rand() % 4;
    int x, y;
    switch (side) {
        case 0: x = rand() % SCREEN_WIDTH; y = SCREEN_HEIGHT + 20; break;
        case 1: x = -20; y = rand() % SCREEN_HEIGHT; break;
        case 2: x = rand() % SCREEN_WIDTH; y = -20; break;
        case 3: x = SCREEN_WIDTH + 20; y = rand() % SCREEN_HEIGHT; break;
    }

    int texIndex;
    if (nextBulletTypeToSpawn == 0) {
        texIndex = side;
        nextBulletTypeToSpawn = 1;
    } else if (nextBulletTypeToSpawn == 1) {
        texIndex = 4;
        nextBulletTypeToSpawn = 2;
    } else {
        texIndex = 5;
        nextBulletTypeToSpawn = 0;
    }

    float dx = playerX - x;
    float dy = playerY - y;
    float len = sqrt(dx * dx + dy * dy);
    float speed = 5 + rand() % 5;
    bullets.emplace_back(x, y, dx / len * speed, dy / len * speed, texIndex);
}

void renderMenu(SDL_Renderer* renderer, SDL_Texture* menuTex, vector<Button>& menuButtons, TTF_Font* font) {
    SDL_RenderCopy(renderer, menuTex, NULL, NULL);
    for (auto& button : menuButtons) {
        button.render(renderer, font);
    }
}

void renderHowToPlay(SDL_Renderer* renderer, SDL_Texture* menuTex, vector<Button>& backButtons,
                     TTF_Font* font, TTF_Font* fontLarge, TTF_Font* fontMedium) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color gold = {255, 215, 0};
    renderText(renderer, fontLarge, "HOW TO PLAY", SCREEN_WIDTH / 2 - 200, 100, gold);

    SDL_Color white = {255, 255, 255};
    renderText(renderer, fontMedium, "CONTROLS:", SCREEN_WIDTH / 2 - 350, 200, white);
    renderText(renderer, font, "RIGHT CLICK - Move player to cursor location", SCREEN_WIDTH / 2 - 350, 250);
    renderText(renderer, font, "ENTER - Place shield in mouse direction (10 second cooldown)", SCREEN_WIDTH / 2 - 350, 300);
    renderText(renderer, font, "ESC - Return to menu during gameplay", SCREEN_WIDTH / 2 - 350, 350);

    renderText(renderer, fontMedium, "GAMEPLAY:", SCREEN_WIDTH / 2 - 350, 420, white);
    renderText(renderer, font, "- Dodge incoming bullets from all directions", SCREEN_WIDTH / 2 - 350, 470);
    renderText(renderer, font, "- Use shields strategically to block bullets", SCREEN_WIDTH / 2 - 350, 520);
    renderText(renderer, font, "- Survive as long as possible to achieve high score", SCREEN_WIDTH / 2 - 350, 570);
    renderText(renderer, font, "- Different bullet types have different movement patterns", SCREEN_WIDTH / 2 - 350, 620);

    for (auto& button : backButtons) {
        button.render(renderer, font);
    }
}

void renderSettings(SDL_Renderer* renderer, SDL_Texture* menuTex, vector<Button>& settingsButtons,
                     TTF_Font* font, TTF_Font* fontLarge, TTF_Font* fontMedium, int musicVolume, int soundVolume) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_Color gold = {255, 215, 0};
    renderText(renderer, fontLarge, "SETTINGS", SCREEN_WIDTH / 2 - 150, 100, gold);
    SDL_Color white = {255, 255, 255};
    renderText(renderer, fontMedium, "Music Volume:", SCREEN_WIDTH / 2 - 250, SCREEN_HEIGHT / 2 - 150, white);
    int musicPercentage = (musicVolume * 100) / MIX_MAX_VOLUME;
    renderText(renderer, fontMedium, intToString(musicPercentage) + "%", SCREEN_WIDTH / 2 + 100, SCREEN_HEIGHT / 2 - 150);
    for (auto& button : settingsButtons) {
        button.render(renderer, font);
    }
}

vector<Button> settingsButtons = {
    Button(SCREEN_WIDTH / 2 - 250, SCREEN_HEIGHT / 2 - 100, 60, 60, "-10%"),
    Button(SCREEN_WIDTH / 2 + 190, SCREEN_HEIGHT / 2 - 100, 60, 60, "+10%"),
    Button(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 100, 200, 40, "BACK TO MENU")
};

void renderHighscore(SDL_Renderer* renderer, SDL_Texture* menuTex, vector<Button>& backButtons,
                     TTF_Font* font, TTF_Font* fontLarge, TTF_Font* fontMedium) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color gold = {255, 215, 0};
    renderText(renderer, fontLarge, "HIGH SCORES", SCREEN_WIDTH / 2 - 200, 100, gold);

    vector<ScoreEntry> scores = loadHighScores();
    SDL_Color white = {255, 255, 255};
    SDL_Color silver = {192, 192, 192};
    SDL_Color bronze = {205, 127, 50};

    renderText(renderer, fontMedium, "RANK", SCREEN_WIDTH / 2 - 350, 200, white);
    renderText(renderer, fontMedium, "NAME", SCREEN_WIDTH / 2 - 150, 200, white);
    renderText(renderer, fontMedium, "SCORE", SCREEN_WIDTH / 2 + 150, 200, white);

    for (size_t i = 0; i < min(scores.size(), (size_t)10); i++) {
        SDL_Color rankColor = white;
        if (i == 0) rankColor = gold;
        else if (i == 1) rankColor = silver;
        else if (i == 2) rankColor = bronze;

        renderText(renderer, font, "#" + intToString(i + 1), SCREEN_WIDTH / 2 - 350, 270 + i * 50, rankColor);
        renderText(renderer, font, scores[i].name, SCREEN_WIDTH / 2 - 150, 270 + i * 50, rankColor);
        renderText(renderer, font, intToString(scores[i].score), SCREEN_WIDTH / 2 + 150, 270 + i * 50, rankColor);
    }

    for (auto& button : backButtons) {
        button.render(renderer, font);
    }
}

void renderGame(SDL_Renderer* renderer, SDL_Texture* bgTex, SDL_Texture* wallTex, vector<Shield>& shields,
                vector<Bullet>& bullets, SDL_Texture* bulletTextures[6], Player& player,
                TTF_Font* font, TTF_Font* fontLarge, TTF_Font* fontMedium,
                int survivalTime, int highScore, int remainingCooldown, GameState gameState) {
    SDL_RenderCopy(renderer, bgTex, NULL, NULL);

    for (auto& shield : shields) {
        shield.render(renderer, wallTex);
    }

    for (auto& b : bullets) {
        b.render(renderer, bulletTextures);
    }

    player.render(renderer);

    SDL_Color white = {255, 255, 255};
    string info = "Time: " + intToString(survivalTime) + "  High Score: " +
    intToString(highScore);
    renderText(renderer, font, info, 10, 10);

    SDL_Color cooldownColor = (remainingCooldown > 0) ? SDL_Color{255, 150, 0} : SDL_Color{0, 255, 0};
    string cooldownText = "Shield Cooldown: " + intToString(remainingCooldown) + "s";
    renderText(renderer, font, cooldownText, 10, 50, cooldownColor);

    if (gameState == GAME_OVER) {
        SDL_Color red = {255, 0, 0};
        renderText(renderer, fontLarge, "GAME OVER", SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2 - 100, red);
        renderText(renderer, fontMedium, "ENTER to restart - ESC for menu", SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 2);
    }
}

int main(int argc, char* argv[]) {
    srand(time(nullptr));

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    SDL_Window* window = SDL_CreateWindow("LOL SIMULATOR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("arial.ttf", 24);
    TTF_Font* fontLarge = TTF_OpenFont("arial.ttf", 72);
    TTF_Font* fontMedium = TTF_OpenFont("arial.ttf", 36);

    SDL_Texture* bgTex = IMG_LoadTexture(renderer, "background.png");
    SDL_Texture* menuTex = IMG_LoadTexture(renderer, "menu.png");
    SDL_Texture* wallTex = IMG_LoadTexture(renderer, "wall.png");
    SDL_Texture* playerTexLeft = IMG_LoadTexture(renderer, "player2.png");
    SDL_Texture* playerTexRight = IMG_LoadTexture(renderer, "player1.png");

    SDL_Texture* bulletTextures[6];
    bulletTextures[0] = IMG_LoadTexture(renderer, "bullet1.1.png");
    bulletTextures[1] = IMG_LoadTexture(renderer, "bullet1.2.png");
    bulletTextures[2] = IMG_LoadTexture(renderer, "bullet1.3.png");
    bulletTextures[3] = IMG_LoadTexture(renderer, "bullet1.4.png");
    bulletTextures[4] = IMG_LoadTexture(renderer, "bullet2.png");
    bulletTextures[5] = IMG_LoadTexture(renderer, "bullet3.png");

    Mix_Music* bgMusic = Mix_LoadMUS("background.wav");
    Mix_Chunk* explosionSound = Mix_LoadWAV("explosion.wav");
    Mix_Chunk* buttonClickSound = Mix_LoadWAV("click.wav");

    int musicVolume = MIX_MAX_VOLUME;
    int soundVolume = MIX_MAX_VOLUME;
    Mix_VolumeMusic(musicVolume);
    Mix_Volume(-1, soundVolume);

    vector<Button> menuButtons = {
        Button(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 300, 200, 40, "PLAY GAME"),
        Button(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 250, 200, 40, "HOW TO PLAY"),
        Button(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 200, 200, 40, "SETTINGS"),
        Button(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 150, 200, 40, "HIGHSCORE"),
        Button(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 100, 200, 40, "EXIT")
    };

    vector<Button> backButtons = {
        Button(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 100, 200, 40, "BACK TO MENU")
    };

    vector<Button> settingsButtons = {
        Button(SCREEN_WIDTH / 2 - 250, SCREEN_HEIGHT / 2 - 100, 60, 60, "-10%"),
        Button(SCREEN_WIDTH / 2 + 190, SCREEN_HEIGHT / 2 - 100, 60, 60, "+10%"),
        Button(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 100, 200, 40, "BACK TO MENU")
    };

    Mix_PlayMusic(bgMusic, -1);

    Player player;
    player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
    player.tex = playerTexRight;

    vector<Bullet> bullets;
    vector<Shield> shields;

    GameState gameState = MENU;

    int targetX = SCREEN_WIDTH / 2, targetY = SCREEN_HEIGHT / 2;
    int survivalTime = 0;
    int highScore = getHighestScore();
    Uint32 lastSpawn = 0, spawnDelay = 500;
    Uint32 startTime = 0;
    Uint32 wallCooldown = 10000, lastWall = 0;
    bool firstShieldUsed = false;
    int remainingCooldown = 0;
    int nextBulletTypeToSpawn = 0;
    int currentMouseX = SCREEN_WIDTH / 2;
    int currentMouseY = SCREEN_HEIGHT / 2;

    SDL_Event e;
    bool quit = false;

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_MOUSEMOTION) {
                SDL_GetMouseState(&currentMouseX, &currentMouseY);
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);

                if (gameState == MENU) {
                    for (size_t i = 0; i < menuButtons.size(); i++) {
                        if (menuButtons[i].isMouseOver(mouseX, mouseY)) {
                            if (buttonClickSound) Mix_PlayChannel(-1, buttonClickSound, 0);

                            if (i == 0) {
                                gameState = PLAYING;
                                resetGame(player, bullets, shields, survivalTime, lastWall, firstShieldUsed, startTime,
                                          nextBulletTypeToSpawn, targetX, targetY, currentMouseX, currentMouseY, playerTexRight);
                            } else if (i == 1) {
                                gameState = HOW_TO_PLAY;
                            } else if (i == 2) {
                                gameState = SETTINGS;
                            } else if (i == 3) {
                                gameState = HIGHSCORE;
                            } else if (i == 4) {
                                quit = true;
                            }
                            break;
                        }
                    }
                } else if (gameState == HOW_TO_PLAY || gameState == HIGHSCORE) {
                    for (auto& button : backButtons) {
                        if (button.isMouseOver(mouseX, mouseY)) {
                            if (buttonClickSound) Mix_PlayChannel(-1, buttonClickSound, 0);
                            gameState = MENU;
                            break;
                        }
                    }
                } else if (gameState == SETTINGS) {
                    for (size_t i = 0; i < settingsButtons.size(); i++) {
                        if (settingsButtons[i].isMouseOver(mouseX, mouseY)) {
                            if (buttonClickSound) Mix_PlayChannel(-1, buttonClickSound, 0);

                            if (i == 0) { // Nút "-10%"
                                musicVolume = max(0, musicVolume - MIX_MAX_VOLUME / 10);
                                Mix_VolumeMusic(musicVolume);
                            } else if (i == 1) { // Nút "+10%"
                                musicVolume = min(MIX_MAX_VOLUME, musicVolume + MIX_MAX_VOLUME / 10);
                                Mix_VolumeMusic(musicVolume);
                            } else if (i == 2) { // Nút "BACK TO MENU"
                                gameState = MENU;
                            }
                            break;
                        }
                    }
                } else if (gameState == PLAYING && e.button.button == SDL_BUTTON_RIGHT) {
                    SDL_GetMouseState(&targetX, &targetY);
                    player.tex = (targetX > player.rect.x + player.rect.w / 2) ? playerTexRight : playerTexLeft;
                }
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    if (gameState == PLAYING || gameState == GAME_OVER ||
                        gameState == HOW_TO_PLAY || gameState == SETTINGS || gameState == HIGHSCORE) {
                        gameState = MENU;
                    }
                } else if (e.key.keysym.sym == SDLK_RETURN) {
                    if (gameState == GAME_OVER) {
                        gameState = PLAYING;
                        resetGame(player, bullets, shields, survivalTime, lastWall, firstShieldUsed, startTime,
                                  nextBulletTypeToSpawn, targetX, targetY, currentMouseX, currentMouseY, playerTexRight);
                    } else if (gameState == PLAYING) {
                        if (!firstShieldUsed || SDL_GetTicks() - lastWall > wallCooldown) {
                            int playerCenterX = player.rect.x + player.rect.w / 2;
                            int playerCenterY = player.rect.y + player.rect.h / 2;

                            float dx = currentMouseX - playerCenterX;
                            float dy = currentMouseY - playerCenterY;
                            bool isHorizontalShield = abs(dx) < abs(dy);

                            shields.push_back(Shield(playerCenterX, playerCenterY, isHorizontalShield));
                            lastWall = SDL_GetTicks();
                            firstShieldUsed = true;
                        }
                    }
                }
            }
        }

        if (gameState == PLAYING) {
            player.moveTo(targetX, targetY);
            Uint32 now = SDL_GetTicks();

            if (firstShieldUsed) {
                remainingCooldown = (wallCooldown - (now - lastWall)) / 1000;
                if (remainingCooldown < 0) remainingCooldown = 0;
            } else {
                remainingCooldown = 0;
            }

            if (now - lastSpawn > spawnDelay) {
                spawnBullet(bullets, player.rect.x, player.rect.y, nextBulletTypeToSpawn);
                lastSpawn = now;
            }

            for (auto& b : bullets) b.move();

            vector<Bullet> remaining;
            for (auto& b : bullets) {
                bool hitShield = false;
                for (auto& s : shields) {
                    if (checkCollision(b.rect, s.rect)) {
                        hitShield = true;
                        break;
                    }
                }
                if (hitShield) continue;
                if (checkCollision(b.rect, player.rect)) {
                    gameState = GAME_OVER;
                    if (survivalTime > highScore) {
                        highScore = survivalTime;
                        updateHighScores(survivalTime);
                    }
                } else {
                    remaining.push_back(b);
                }
            }
            bullets = remaining;
            shields.erase(remove_if(shields.begin(), shields.end(), [](Shield& s) { return s.isExpired(); }), shields.end());
            survivalTime = (SDL_GetTicks() - startTime) / 1000;
        }

        SDL_RenderClear(renderer);

        switch (gameState) {
            case MENU:
                renderMenu(renderer, menuTex, menuButtons, font);
                break;
            case HOW_TO_PLAY:
                renderHowToPlay(renderer, menuTex, backButtons, font, fontLarge, fontMedium);
                break;
            case SETTINGS:
                renderSettings(renderer, menuTex, settingsButtons, font, fontLarge, fontMedium, musicVolume, soundVolume);
                break;
            case HIGHSCORE:
                renderHighscore(renderer, menuTex, backButtons, font, fontLarge, fontMedium);
                break;
            case PLAYING:
            case GAME_OVER:
                renderGame(renderer, bgTex, wallTex, shields, bullets, bulletTextures, player, font, fontLarge, fontMedium,
                           survivalTime, highScore, remainingCooldown, gameState);
                break;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    Mix_FreeMusic(bgMusic);
    Mix_FreeChunk(explosionSound);
    if (buttonClickSound) Mix_FreeChunk(buttonClickSound);

    SDL_DestroyTexture(bgTex);
    SDL_DestroyTexture(menuTex);
    SDL_DestroyTexture(wallTex);
    SDL_DestroyTexture(playerTexLeft);
    SDL_DestroyTexture(playerTexRight);

    for (int i = 0; i < 6; ++i) {
        SDL_DestroyTexture(bulletTextures[i]);
    }

    TTF_CloseFont(font);
    TTF_CloseFont(fontLarge);
    TTF_CloseFont(fontMedium);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    Mix_Quit();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
