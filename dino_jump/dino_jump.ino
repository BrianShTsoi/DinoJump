#include "bit_array_2d.hpp"
#include "game.hpp"
#include "hologram_fan.hpp"

// Define the cores
#define CORE_0 0
#define CORE_1 1

#define RESET_PIN D7
#define IR_PIN D8

void TaskDisplay(void *pvParameters);
void TaskGame(void *pvParameters);

void setup() {
	Wire.begin();
	Wire.setClock(1000000);

	pinMode(RESET_PIN, OUTPUT);
	digitalWrite(RESET_PIN, LOW);
	delay(5);
	digitalWrite(RESET_PIN, HIGH);

	Game *game = new Game();

	xTaskCreatePinnedToCore(TaskDisplay, "Display Task", 8192, game, 1, NULL, CORE_1);
	xTaskCreatePinnedToCore(TaskGame, "Game Task", 8192, game, 2, NULL, CORE_0);
}

void loop() {}

void TaskDisplay(void *pvParameters) {
	Game *game = static_cast<Game *>(pvParameters);
	HologramFan display = HologramFan();

	while (true) {
		if (digitalRead(IR_PIN) == LOW) {
			display.flash_frame(game->get_frame());
		}

		// Need to determine if this delay causes use to miss flashing frames
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	vTaskDelete(NULL);
}

void TaskGame(void *pvParameters) {
	Game *game = static_cast<Game *>(pvParameters);

	while (true) {
		game->update_obstacles();
		game->update_frame();

		vTaskDelay(pdMS_TO_TICKS(100));
	}

	vTaskDelete(NULL);
}
