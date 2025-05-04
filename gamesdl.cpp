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

// Định nghĩa các trạng thái game
enum GameState {
    MENU,       // Menu chính
    PLAYING,    // Đang chơi
    GAME_OVER   // Game over
};

struct Button {
    SDL_Rect rect;
    string text;

    Button(int x, int y, int w, int h, const string& t) : rect{x, y, w, h}, text(t) {}

    bool isMouseOver(int x, int y) {
        return (x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h);
    }

    void render(SDL_Renderer* renderer, TTF_Font* font) {
        // Vẽ nền nút
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 200);
        SDL_RenderFillRect(renderer, &rect);

        // Vẽ viền nút
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &rect);

        // Vẽ text
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
            rect = {x - 50, y - 10, 100, 20}; // Khiên ngang
        else
            rect = {x - 10, y - 50, 20, 100}; // Khiên dọc
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

void renderText(SDL_Renderer* renderer, TTF_Font* font, const string& text, int x, int y, SDL_Color color = {255, 255, 255}) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
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
    // Load bullet type 1 textures (các ảnh xoay của nhau)
    bulletTextures[0] = IMG_LoadTexture(renderer, "bullet1.1.png"); // Từ dưới lên
    bulletTextures[1] = IMG_LoadTexture(renderer, "bullet1.2.png"); // Từ trái sang
    bulletTextures[2] = IMG_LoadTexture(renderer, "bullet1.3.png"); // Từ trên xuống
    bulletTextures[3] = IMG_LoadTexture(renderer, "bullet1.4.png"); // Từ phải sang
    // Load bullet type 2 and 3
    bulletTextures[4] = IMG_LoadTexture(renderer, "bullet2.png");
    bulletTextures[5] = IMG_LoadTexture(renderer, "bullet3.png");

    Mix_Music* bgMusic = Mix_LoadMUS("background.wav");
    Mix_Chunk* explosionSound = Mix_LoadWAV("explosion.wav");
    Mix_Chunk* buttonClickSound = Mix_LoadWAV("click.wav");  // Tạo sound click (nếu có)

    // Tạo các nút cho menu
    vector<Button> menuButtons = {
        Button(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2, 300, 60, "PLAY GAME"),
        Button(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 100, 300, 60, "EXIT")
    };

    Mix_PlayMusic(bgMusic, -1);

    Player player;
    player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
    player.tex = playerTexRight;

    vector<Bullet> bullets;
    vector<Shield> shields;

    // Khởi tạo game state là MENU
    GameState gameState = MENU;

    int targetX = SCREEN_WIDTH / 2, targetY = SCREEN_HEIGHT / 2;
    int survivalTime = 0;
    int highScore = loadHighScore();
    Uint32 lastSpawn = 0, spawnDelay = 500;
    Uint32 startTime = 0;
    Uint32 wallCooldown = 10000, lastWall = 0;
    bool firstShieldUsed = false; // Biến để theo dõi lần sử dụng đầu tiên
    int remainingCooldown = 0; // Biến để hiển thị thời gian hồi chiêu còn lại

    // Biến để theo dõi loại đạn tiếp theo sẽ được bắn
    int nextBulletTypeToSpawn = 0; // 0: bullet1, 1: bullet2, 2: bullet3

    // Biến để lưu vị trí chuột hiện tại
    int currentMouseX = SCREEN_WIDTH / 2;
    int currentMouseY = SCREEN_HEIGHT / 2;

    SDL_Event e;
    bool quit = false;

    while (!quit) {
        // Xử lý các sự kiện
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            else if (e.type == SDL_MOUSEMOTION) {
                // Cập nhật vị trí chuột hiện tại
                SDL_GetMouseState(&currentMouseX, &currentMouseY);
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);

                if (gameState == MENU) {
                    // Kiểm tra xem người chơi có click vào nút nào không
                    for (size_t i = 0; i < menuButtons.size(); i++) {
                        if (menuButtons[i].isMouseOver(mouseX, mouseY)) {
                            if (buttonClickSound) {
                                Mix_PlayChannel(-1, buttonClickSound, 0);
                            }

                            if (i == 0) { // PLAY GAME
                                gameState = PLAYING;
                                // Reset game khi bắt đầu chơi mới
                                bullets.clear();
                                shields.clear();
                                player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
                                player.tex = playerTexRight;
                                survivalTime = 0;
                                lastWall = 0;
                                firstShieldUsed = false;  // Reset biến khi bắt đầu game mới
                                startTime = SDL_GetTicks();
                                nextBulletTypeToSpawn = 0;
                                targetX = player.rect.x;
                                targetY = player.rect.y;
                                currentMouseX = targetX;
                                currentMouseY = targetY;
                            }
                            else if (i == 1) { // EXIT
                                quit = true;
                            }
                            break;
                        }
                    }
                }
                else if (gameState == PLAYING && e.button.button == SDL_BUTTON_RIGHT) {
                    SDL_GetMouseState(&targetX, &targetY);

                    if (targetX > player.rect.x + player.rect.w / 2) {
                        player.tex = playerTexRight;
                    } else {
                        player.tex = playerTexLeft;
                    }
                }
            }
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    // Nếu đang chơi, ESC sẽ quay về menu
                    if (gameState == PLAYING || gameState == GAME_OVER) {
                        gameState = MENU;
                    }
                }
                else if (e.key.keysym.sym == SDLK_RETURN) {
                    if (gameState == GAME_OVER) {
                        gameState = PLAYING;
                        bullets.clear();
                        shields.clear();
                        player.rect = {SCREEN_WIDTH / 2 - 30, SCREEN_HEIGHT / 2 - 30, 60, 60};
                        player.tex = playerTexRight;
                        survivalTime = 0;
                        lastWall = 0;
                        firstShieldUsed = false;  // Reset biến khi bắt đầu game mới
                        startTime = SDL_GetTicks();
                        targetX = player.rect.x;
                        targetY = player.rect.y;
                        currentMouseX = targetX;
                        currentMouseY = targetY;
                        nextBulletTypeToSpawn = 0; // Reset loại đạn về bullet1
                    }
                    else if (gameState == PLAYING) {
                        // Kiểm tra lần sử dụng đầu tiên hoặc đã hết thời gian hồi chiêu
                        if (!firstShieldUsed || SDL_GetTicks() - lastWall > wallCooldown) {
                            // Lấy vị trí trung tâm của người chơi
                            int playerCenterX = player.rect.x + player.rect.w / 2;
                            int playerCenterY = player.rect.y + player.rect.h / 2;

                            // Tính hướng từ người chơi đến vị trí chuột hiện tại
                            float dx = currentMouseX - playerCenterX;
                            float dy = currentMouseY - playerCenterY;

                            // Xác định xem khiên là ngang hay dọc dựa trên hướng chuột
                            // Nếu góc giữa vector và trục x là nhỏ (dx lớn hơn dy theo giá trị tuyệt đối),
                            // thì set khiên là dọc (vertical). Ngược lại, set khiên là ngang (horizontal).
                            bool isHorizontalShield = abs(dx) < abs(dy);

                            // Đặt lá chắn vào vị trí của người chơi, với hướng dựa vào vị trí chuột
                            shields.push_back(Shield(playerCenterX, playerCenterY, isHorizontalShield));

                            lastWall = SDL_GetTicks();
                            firstShieldUsed = true; // Đánh dấu đã sử dụng lá chắn lần đầu
                        }
                    }
                }
            }
        }

        // Cập nhật game logic dựa vào trạng thái hiện tại
        if (gameState == PLAYING) {
            player.moveTo(targetX, targetY);

            Uint32 now = SDL_GetTicks();

            // Cập nhật thời gian hồi chiêu còn lại
            if (firstShieldUsed) {
                remainingCooldown = (wallCooldown - (now - lastWall)) / 1000;
                if (remainingCooldown < 0) remainingCooldown = 0;
            } else {
                remainingCooldown = 0;
            }

            if (now - lastSpawn > spawnDelay) {
                int side = rand() % 4;
                int x, y;
                switch (side) {
                    case 0: x = rand() % SCREEN_WIDTH; y = SCREEN_HEIGHT + 20; break; // Từ dưới lên
                    case 1: x = -20; y = rand() % SCREEN_HEIGHT; break;              // Từ trái sang
                    case 2: x = rand() % SCREEN_WIDTH; y = -20; break;               // Từ trên xuống
                    case 3: x = SCREEN_WIDTH + 20; y = rand() % SCREEN_HEIGHT; break; // Từ phải sang
                }

                // Xác định chỉ số texture dựa trên loại đạn tiếp theo và phía spawn
                int texIndex;

                if (nextBulletTypeToSpawn == 0) {
                    // Nếu là bullet1, chọn texture dựa vào phía spawn
                    texIndex = side; // 0: từ dưới, 1: từ trái, 2: từ trên, 3: từ phải
                    nextBulletTypeToSpawn = 1; // Loại đạn tiếp theo là bullet2
                }
                else if (nextBulletTypeToSpawn == 1) {
                    texIndex = 4; // bullet2.png
                    nextBulletTypeToSpawn = 2; // Loại đạn tiếp theo là bullet3
                }
                else {
                    texIndex = 5; // bullet3.png
                    nextBulletTypeToSpawn = 0; // Quay lại loại đạn bullet1
                }

                float dx = player.rect.x - x;
                float dy = player.rect.y - y;
                float len = sqrt(dx * dx + dy * dy);
                float speed = 5 + rand() % 5;
                bullets.emplace_back(x, y, dx / len * speed, dy / len * speed, texIndex);
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
                    gameState = GAME_OVER;
                    if (survivalTime > highScore) {
                        highScore = survivalTime;
                        saveHighScore(highScore);
                    }
                } else {
                    remaining.push_back(b);
                }
            }
            bullets = remaining;
            shields.erase(remove_if(shields.begin(), shields.end(),
                          [](Shield& s) { return s.isExpired(); }), shields.end());
            survivalTime = (SDL_GetTicks() - startTime) / 1000;
        }

        // Render game dựa vào trạng thái hiện tại
        SDL_RenderClear(renderer);

        if (gameState == MENU) {
            // Vẽ menu
            SDL_RenderCopy(renderer, menuTex, NULL, NULL);

            // Vẽ tiêu đề game
            SDL_Color gold = {255, 215, 0};
            renderText(renderer, fontLarge, "LOL SIMULATOR",
                      SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 4, gold);

            // Vẽ điểm cao nhất
            string highScoreText = "HIGH SCORE: " + intToString(highScore);
            renderText(renderer, fontMedium, highScoreText,
                      SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 3 + 50);

            // Vẽ các nút menu
            for (auto& button : menuButtons) {
                button.render(renderer, font);
            }

            // Vẽ hướng dẫn
            SDL_Color lightBlue = {173, 216, 230};
            renderText(renderer, font, "CONTROLS:",
                      SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT - 180, lightBlue);
            renderText(renderer, font, "RIGHT CLICK - Move player",
                      SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT - 150);
            renderText(renderer, font, "ENTER - Place shield in mouse direction",
                      SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT - 120);
            renderText(renderer, font, "ESC - Return to menu",
                      SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT - 90);
        }
        else {
            // Vẽ background và game objects
            SDL_RenderCopy(renderer, bgTex, NULL, NULL);

            for (auto& shield : shields) {
                shield.render(renderer, wallTex);
            }

            for (auto& b : bullets) {
                b.render(renderer, bulletTextures);
            }

            player.render(renderer);

            // Vẽ thông tin điểm
            SDL_Color white = {255, 255, 255};
            string info = "Time: " + intToString(survivalTime) + "   High Score: " + intToString(highScore);
            renderText(renderer, font, info, 10, 10);

            // Hiển thị thời gian hồi chiêu của lá chắn
            SDL_Color cooldownColor;
            if (remainingCooldown > 0) {
                cooldownColor = {255, 150, 0}; // Màu cam khi đang hồi chiêu
            } else {
                cooldownColor = {0, 255, 0};   // Màu xanh lá khi sẵn sàng
            }
            string cooldownText = "Shield Cooldown: " + intToString(remainingCooldown) + "s";
            renderText(renderer, font, cooldownText, 10, 50, cooldownColor);

            // Vẽ màn hình game over nếu cần
            if (gameState == GAME_OVER) {
                SDL_Color red = {255, 0, 0};
                renderText(renderer, fontLarge, "GAME OVER",
                          SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2 - 100, red);
                renderText(renderer, fontMedium, "ENTER to restart - ESC for menu",
                          SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 2);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    // Giải phóng bộ nhớ
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
