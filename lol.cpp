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
    SDL_Texture* tex;
    float speed = 5.0f;  // Tốc độ di chuyển

    void moveTo(int x, int y) {
        // Tính toán hướng di chuyển từ vị trí hiện tại tới vị trí chuột
        float dx = x - rect.x - rect.w / 2;
        float dy = y - rect.y - rect.h / 2;
        float distance = sqrt(dx * dx + dy * dy);

        // Nếu khoảng cách còn lớn hơn một ngưỡng, tiếp tục di chuyển
        if (distance > speed) {
            // Tính toán hướng di chuyển
            dx /= distance;
            dy /= distance;

            // Di chuyển nhân vật từ từ về phía chuột
            rect.x += dx * speed;
            rect.y += dy * speed;
        } else {
            // Nếu gần đến chuột, dừng lại ở vị trí chuột
            rect.x = x - rect.w / 2;
            rect.y = y - rect.h / 2;
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
    Mix_Chunk* bulletSound = Mix_LoadWAV("bullet.wav");
    Mix_Chunk* explosionSound = Mix_LoadWAV("explosion.wav");

    Mix_PlayMusic(bgMusic, -1);

    Player player;
    player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
    player.tex = playerTexRight;

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
                    player.tex = playerTexRight;
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
                SDL_GetMouseState(&targetX, &targetY); // Lấy vị trí chuột khi nhấn
                if (targetX > player.rect.x + player.rect.w / 2) {
                    player.tex = playerTexRight;  // Hướng nhân vật sang phải
                } else {
                    player.tex = playerTexLeft;  // Hướng nhân vật sang trái
                }
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RETURN) {
                    if (gameOver) {
                        showStart = false;
                        gameOver = false;
                        bullets.clear();
                        shields.clear();
                        player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
                        player.tex = playerTexRight;
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
                int texIndex = bulletTexIndex;

                switch (side) {
                    case 0: x = rand() % SCREEN_WIDTH; y = -20; break;
                    case 1: x = SCREEN_WIDTH + 20; y = rand() % SCREEN_HEIGHT; break;
                    case 2: x = rand() % SCREEN_WIDTH; y = SCREEN_HEIGHT + 20; break;
                    case 3: x = -20; y = rand() % SCREEN_HEIGHT; break;
                }

                bullets.push_back(Bullet(x, y, rand() % 2 == 0 ? rand() % 2 + 4 : rand() % 2 + 2, rand() % 2 + 2, texIndex));
                lastSpawn = now;
            }

            // Xử lý đạn di chuyển và kiểm tra va chạm
            for (auto& bullet : bullets) {
                bullet.move();
            }

            // Xử lý khiên hết thời gian
            for (auto& shield : shields) {
                if (shield.isExpired()) {
                    shields.erase(remove(shields.begin(), shields.end(), shield), shields.end());
                    break;
                }
            }
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, bgTex, NULL, NULL);

        // Render đối tượng
        player.render(renderer);

        for (auto& bullet : bullets) {
            bullet.render(renderer, bulletTextures);
        }

        for (auto& shield : shields) {
            shield.render(renderer, wallTex);
        }

        if (gameOver) {
            SDL_Color white = {255, 255, 255};
            SDL_Surface* surface = TTF_RenderText_Solid(fontLarge, "Game Over", white);
            SDL_Texture* text = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect rect = {(SCREEN_WIDTH - surface->w) / 2, SCREEN_HEIGHT / 2 - surface->h / 2, surface->w, surface->h};
            SDL_RenderCopy(renderer, text, NULL, &rect);
            SDL_FreeSurface(surface);
            SDL_DestroyTexture(text);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    saveHighScore(highScore);
    SDL_DestroyTexture(bgTex);
    SDL_DestroyTexture(playerTexLeft);
    SDL_DestroyTexture(playerTexRight);
    for (int i = 0; i < 6; i++) {
        SDL_DestroyTexture(bulletTextures[i]);
    }
    Mix_FreeMusic(bgMusic);
    Mix_FreeChunk(bulletSound);
    Mix_FreeChunk(explosionSound);
    TTF_CloseFont(font);
    TTF_CloseFont(fontLarge);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
