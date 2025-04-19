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
#include <fstream> // Để đọc/ghi file

using namespace std;

const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 800;

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

    void render(SDL_Renderer* renderer, SDL_Texture* bulletTextures[3]) {
        SDL_RenderCopy(renderer, bulletTextures[texIndex], NULL, &rect);
    }
};

struct Player {
    SDL_Rect rect;
    int speed = 4;

    Player() {
        rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
    }

    void moveTo(int targetX, int targetY) {
        float dx = targetX - rect.x;
        float dy = targetY - rect.y;
        float dist = sqrt(dx * dx + dy * dy);
        if (dist > speed) {
            rect.x += (dx / dist) * speed;
            rect.y += (dy / dist) * speed;
        }
    }

    void render(SDL_Renderer* renderer, SDL_Texture* tex) {
        SDL_RenderCopy(renderer, tex, NULL, &rect);
    }
};

struct Shield {
    SDL_Rect rect;
    Uint32 spawnTime;

    Shield(int x, int y, bool horizontal) {
        if (horizontal)
            rect = {x - 50, y - 10, 20, 100};
        else
            rect = {x - 10, y - 50, 100, 20};
        spawnTime = SDL_GetTicks();
    }

    bool isExpired() {
        return SDL_GetTicks() - spawnTime > 2000;
    }

    void render(SDL_Renderer* renderer, SDL_Texture* tex) {
        SDL_RenderCopy(renderer, tex, NULL, &rect);
    }
};

bool checkCollision(SDL_Rect a, SDL_Rect b) {
    return SDL_HasIntersection(&a, &b);
}

string intToString(int n) {
    return to_string(n);
}

int loadHighScore() {
    ifstream inFile("highscore.txt");
    int score = 0;
    if (inFile.is_open()) {
        inFile >> score;
        inFile.close();
    }
    return score;
}

void saveHighScore(int score) {
    ofstream outFile("highscore.txt");
    if (outFile.is_open()) {
        outFile << score;
        outFile.close();
    }
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    SDL_Window* window = SDL_CreateWindow("Rocket Dodge", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("arial.ttf", 24);
    TTF_Font* fontLarge = TTF_OpenFont("arial.ttf", 72);

    SDL_Texture* bgTex = IMG_LoadTexture(renderer, "background.png");
    SDL_Texture* wallTex = IMG_LoadTexture(renderer, "wall.png");
    SDL_Texture* playerTex = IMG_LoadTexture(renderer, "player.png");

    SDL_Texture* bulletTextures[3];
    bulletTextures[0] = IMG_LoadTexture(renderer, "bullet1.png");
    bulletTextures[1] = IMG_LoadTexture(renderer, "bullet2.png");
    bulletTextures[2] = IMG_LoadTexture(renderer, "bullet3.png");

    Mix_Music* bgMusic = Mix_LoadMUS("background.wav");
    Mix_Chunk* bulletSound = Mix_LoadWAV("bullet.wav");
    Mix_Chunk* explosionSound = Mix_LoadWAV("explosion.wav");

    Mix_PlayMusic(bgMusic, -1);

    Player player;
    vector<Bullet> bullets;
    vector<Shield> shields;

    bool quit = false, gameOver = false, showStart = true;
    int targetX = SCREEN_WIDTH / 2, targetY = SCREEN_HEIGHT / 2;
    int survivalTime = 0;
    int highScore = loadHighScore();
    Uint32 lastSpawn = 0, spawnDelay = 500;
    Uint32 startTime = SDL_GetTicks();
    Uint32 wallCooldown = 10000, lastWall = 0;
    int bulletTexIndex = 0;

    SDL_Event e;

    while (!quit) {
        if (showStart) {
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, bgTex, NULL, NULL);
            SDL_Color white = {255, 255, 255};
            SDL_Surface* surface = TTF_RenderText_Solid(fontLarge, "Rocket Dodge", white);
            SDL_Texture* text = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect rect = {(SCREEN_WIDTH - surface->w) / 2, SCREEN_HEIGHT / 2 - surface->h / 2, surface->w, surface->h};
            SDL_RenderCopy(renderer, text, NULL, &rect);
            SDL_FreeSurface(surface);
            SDL_DestroyTexture(text);
            SDL_RenderPresent(renderer);
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) quit = true;
                else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) {
                    showStart = false;
                    gameOver = false;
                    bullets.clear();
                    shields.clear();
                    player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
                    survivalTime = 0;
                    lastWall = 0;
                    startTime = SDL_GetTicks();
                    targetX = player.rect.x;
                    targetY = player.rect.y;
                }
            }
            SDL_Delay(16);
            continue;
        }

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_GetMouseState(&targetX, &targetY);
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RETURN) {
                    if (gameOver) {
                        gameOver = false;
                        bullets.clear();
                        shields.clear();
                        player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
                        survivalTime = 0;
                        lastWall = 0;
                        startTime = SDL_GetTicks();
                        targetX = player.rect.x;
                        targetY = player.rect.y;
                    } else {
                        if (SDL_GetTicks() - lastWall > wallCooldown) {
                            shields.push_back(Shield(player.rect.x, player.rect.y, rand() % 2 == 0));
                            lastWall = SDL_GetTicks();
                        }
                    }
                }
            }
        }

        if (!gameOver) {
            player.moveTo(targetX, targetY);

            Uint32 now = SDL_GetTicks();
            if (now - lastSpawn > spawnDelay) {
                int side = rand() % 4;
                int x, y;
                switch (side) {
                    case 0: x = rand() % SCREEN_WIDTH; y = -20; break;
                    case 1: x = SCREEN_WIDTH + 20; y = rand() % SCREEN_HEIGHT; break;
                    case 2: x = rand() % SCREEN_WIDTH; y = SCREEN_HEIGHT + 20; break;
                    default: x = -20; y = rand() % SCREEN_HEIGHT; break;
                }
                float dx = player.rect.x - x;
                float dy = player.rect.y - y;
                float len = sqrt(dx * dx + dy * dy);
                float speed = 5 + rand() % 5;
                bullets.emplace_back(x, y, dx / len * speed, dy / len * speed, bulletTexIndex);
                bulletTexIndex = (bulletTexIndex + 1) % 3;
                Mix_PlayChannel(-1, bulletSound, 0);
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
                    Mix_PlayChannel(-1, explosionSound, 0);
                    gameOver = true;
                    if (survivalTime > highScore) {
                        highScore = survivalTime;
                        saveHighScore(highScore);  // Ghi vào file
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
        SDL_RenderCopy(renderer, bgTex, NULL, NULL);
        for (auto& shield : shields) shield.render(renderer, wallTex);
        for (auto& b : bullets) b.render(renderer, bulletTextures);
        player.render(renderer, playerTex);

        SDL_Color white = {255, 255, 255};
        string info = "Time: " + intToString(survivalTime) + "   High Score: " + intToString(highScore);
        if (gameOver && survivalTime == highScore && survivalTime > 0) info += "   New High Score!";
        SDL_Surface* infoSurface = TTF_RenderText_Solid(font, info.c_str(), white);
        SDL_Texture* infoTex = SDL_CreateTextureFromSurface(renderer, infoSurface);
        SDL_Rect infoRect = {10, 10, infoSurface->w, infoSurface->h};
        SDL_RenderCopy(renderer, infoTex, NULL, &infoRect);
        SDL_FreeSurface(infoSurface); SDL_DestroyTexture(infoTex);

        string cdText = (SDL_GetTicks() - lastWall >= wallCooldown) ? "Wall Cooldown: Ready" :
            "Wall Cooldown: " + intToString((wallCooldown - (SDL_GetTicks() - lastWall)) / 1000) + "s";
        SDL_Surface* cdSurface = TTF_RenderText_Solid(font, cdText.c_str(), white);
        SDL_Texture* cdTex = SDL_CreateTextureFromSurface(renderer, cdSurface);
        SDL_Rect cdRect = {SCREEN_WIDTH - cdSurface->w - 10, 10, cdSurface->w, cdSurface->h};
        SDL_RenderCopy(renderer, cdTex, NULL, &cdRect);
        SDL_FreeSurface(cdSurface); SDL_DestroyTexture(cdTex);

        if (gameOver) {
            const char* lines[] = {
                "Game Over",
                ("Time Survived: " + intToString(survivalTime) + " seconds").c_str()
            };
            for (int i = 0; i < 2; i++) {
                SDL_Surface* surface = TTF_RenderText_Solid(font, lines[i], white);
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect dstRect = {(SCREEN_WIDTH - surface->w) / 2, SCREEN_HEIGHT / 2 + i * 30, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, NULL, &dstRect);
                SDL_FreeSurface(surface);
                SDL_DestroyTexture(texture);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    Mix_FreeMusic(bgMusic);
    Mix_FreeChunk(bulletSound);
    Mix_FreeChunk(explosionSound);
    TTF_CloseFont(font);
    TTF_CloseFont(fontLarge);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    Mix_Quit();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
