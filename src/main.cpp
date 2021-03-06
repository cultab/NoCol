#include <SFML/Graphics.hpp>
#include <vector>
#include <list>
#include <cmath>
#include <iostream>
#include <fstream>

#include "display_manager.hpp"


int WIN_WIDTH = 1920;
int WIN_HEIGHT = 1080;
const uint32_t max_history = 100;


float dot(const sf::Vector2f& v1, const sf::Vector2f& v2)
{
	return v1.x * v2.x + v1.y * v2.y;
}

float length(const sf::Vector2f& v)
{
	return sqrt(v.x * v.x + v.y * v.y);
}

sf::Vector2f normalize(const sf::Vector2f& v)
{
	return v / length(v);
}


struct Ball
{
	sf::Vector2f position, velocity;
	float r;

	std::vector<sf::Vector2f> position_history;
	uint32_t current_idx;

    bool stable;
    int stableCount;

	Ball()
		: position_history(max_history)
		, current_idx(0)
	{
	}

	Ball(float x, float y, float arg_r)
		: position(x, y)
		, velocity(static_cast<float>(rand() % 8 - 4), static_cast<float>(rand() % 8 - 4))
		, r(arg_r)
		, position_history(max_history)
		, current_idx(0)
    {
		for (sf::Vector2f& pos : position_history) {
			pos = sf::Vector2f(x, y);
		}
        stableCount = 0;
    }

    void save()
    {
		position_history[current_idx] = position;
		current_idx = (++current_idx) % max_history;
    }

	sf::VertexArray getVA() const
	{
		sf::VertexArray va(sf::LineStrip, max_history);
		for (uint32_t i(0); i < max_history; ++i) {
			const uint32_t actual_idx = (i + current_idx) % max_history;
			const float ratio = i / float(max_history);
			va[i].position = position_history[actual_idx];
			va[i].color = sf::Color(0, static_cast<sf::Uint8>(255 * ratio), 0);
		}

		return va;
	}
};

bool update(std::vector<Ball>& balls, float speed)
{
    bool stable = true;

    const uint32_t nBalls = static_cast<uint32_t>(balls.size());
	const float dt = 0.008f;
	const float attraction_force = 50.0f;
	const float attraction_force_bug = 0.002f;
	const sf::Vector2f center_position(WIN_WIDTH * 0.5f, WIN_HEIGHT * 0.5f);
	const float attractor_threshold = 50.0f;

    for (uint32_t i(0); i<nBalls; i++) {
	    Ball& current_ball = balls[i];
		// Attraction to center
		const sf::Vector2f to_center = center_position - current_ball.position;
		const float dist_to_center = length(to_center);
		current_ball.velocity += attraction_force_bug * to_center;

		for (uint32_t k=i+1; k<nBalls; k++) {
		    Ball& collider = balls[k];
			const sf::Vector2f collide_vec = current_ball.position - collider.position;
			const float dist = sqrt(collide_vec.x*collide_vec.x + collide_vec.y*collide_vec.y);

			const float minDist = current_ball.r+collider.r;

			if (dist < minDist) {
			    stable = false;

			    current_ball.stable = false;
			    collider.stable = false;

				const sf::Vector2f collide_axe = collide_vec / dist;

				current_ball.position += 0.5f * (minDist - dist) * collide_axe;
				collider.position -= 0.5f * (minDist - dist) * collide_axe;
			}
		}
	}

	for (uint32_t i(0); i<nBalls; i++)
    {
        if(balls[i].stable)
            balls[i].stableCount++;
        else
            balls[i].stableCount = 0;
    }

	return stable;
}

void updatePos(std::vector<Ball>& balls, float speedDownFactor, float& speedDownCounter)
{
	const float dt = 0.016f;
    for (Ball& currentBall : balls) {
       currentBall.position += (dt / speedDownFactor) * currentBall.velocity;
    }

    speedDownCounter--;
}

const Ball* getBallAt(const sf::Vector2f& position, std::vector<Ball>& balls)
{
	for (const Ball& ball : balls) {
		const sf::Vector2f v = position - ball.position;
		const float dist = sqrt(v.x*v.x + v.y * v.y);
		if (dist < ball.r) {
			return &ball;
		}
	}

	return nullptr;
}


int main()
{
	const uint32_t seed = static_cast<uint32_t>(time(0));
    srand(seed);
	std::cout << seed << std::endl;
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode(WIN_WIDTH, WIN_HEIGHT), "NoCol", sf::Style::Fullscreen, settings);
    window.setVerticalSyncEnabled(true);

    float speedDownFactor = 1;
    float speedDownCounter = 1;
    float waitingSpeedFactor = 1;
    float speedDownFactorGoal = 1;
    int iterations = 0;

    bool drawTraces = true;
    bool synccEnable = true;

    int nBalls = 80;
    int maxSize = 12;
    int minSize = 5;

    std::ifstream infile;
    infile.open("config");
    std::cout << "Loading config" << std::endl;
    infile >> nBalls;
    infile >> maxSize;
    infile >> minSize;

	const float spawn_range_factor = 0.5f;
    std::vector<Ball> balls;
	for (int i(0); i < nBalls; i++) {
		const float angle = (rand() % 10000) / 10000.0f * 2.0f * 3.141592653f;
		const float radius = 450.0f;

		const float start_x = radius * cos(angle);
		const float start_y = radius * sin(angle);

		const float speed = ((rand() % 2) ? 1.0f : -1.0f) * (rand()%20 + 150);

		balls.push_back(Ball(start_x + WIN_WIDTH * 0.5f, start_y + WIN_HEIGHT * 0.5f,
			static_cast<float>(rand() % (maxSize - minSize) + minSize)));

		balls.back().velocity.x = -sin(angle) * speed;
		balls.back().velocity.y = cos(angle) * speed;
	}

	const float close_threshold = 50.0f;

    sf::RenderTexture traces, blurTexture, renderer;
    blurTexture.create(WIN_WIDTH, WIN_HEIGHT);
    traces.create(WIN_WIDTH, WIN_HEIGHT);
    renderer.create(WIN_WIDTH, WIN_HEIGHT);

    traces.clear(sf::Color::Black);
    traces.display();

	DisplayManager display_manager(window);
	display_manager.event_manager.addKeyReleasedCallback(sf::Keyboard::A, [&](sfev::CstEv) { drawTraces = !drawTraces; });
	display_manager.event_manager.addKeyReleasedCallback(sf::Keyboard::C, [&](sfev::CstEv) { traces.clear(); });
	display_manager.event_manager.addKeyReleasedCallback(sf::Keyboard::Space, [&](sfev::CstEv) {  
		speedDownFactorGoal = speedDownFactor == 1 ? 10.0f : 1.0f;
	});
	display_manager.event_manager.addKeyReleasedCallback(sf::Keyboard::Escape, [&](sfev::CstEv) {window.close(); });
	display_manager.event_manager.addKeyReleasedCallback(sf::Keyboard::E, [&](sfev::CstEv) { 
		synccEnable = !synccEnable;
		window.setVerticalSyncEnabled(synccEnable);
	});

	const Ball* focus = nullptr;

	uint32_t ok_count = 0;
 
    while (window.isOpen()) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

		std::vector<sf::Event> events = display_manager.processEvents();
		const sf::RenderStates rs = display_manager.getRenderStates();

		if (waitingSpeedFactor != speedDownFactorGoal) {
			waitingSpeedFactor += speedDownFactorGoal - waitingSpeedFactor;
		}

		if (display_manager.clic) {
			focus = getBallAt(display_manager.getWorldMousePosition(), balls);
			display_manager.clic = false;
		}

		if (focus) {
			display_manager.setOffset(focus->position.x, focus->position.y);
		}

		bool stable = true;
		if (!speedDownCounter) {
			for (Ball& ball : balls) {
				ball.stable = true;
				ball.save();
			}

			stable = update(balls, 1);
			if (!stable && ok_count < 200) {
				ok_count = 0;
			}

			if (stable) {
				++ok_count;
			}

			if (waitingSpeedFactor) {
				speedDownFactor = waitingSpeedFactor;
			}
			speedDownCounter = speedDownFactor;
		}

		updatePos(balls, speedDownFactor, speedDownCounter);

        window.clear(sf::Color::Black);

		sf::RenderStates rs_traces = rs;
		rs_traces.blendMode = sf::BlendAdd;
		for (const Ball& b : balls) {
			if (drawTraces) {
				sf::VertexArray trace = b.getVA();
				window.draw(trace, rs_traces);
			}
		}

        for (const Ball& b : balls)
        {
            int c = b.stableCount > 255 ? 255 : b.stableCount;
            sf::Color color = ok_count >= 200 ? sf::Color::Green : sf::Color(255 - c, c, 0);
            float r = b.r;

            if (speedDownFactor > 1)
                r = b.r;

            sf::CircleShape ballRepresentation(r);
			ballRepresentation.setPointCount(128);
            ballRepresentation.setFillColor(color);
            ballRepresentation.setOrigin(r, r);
            ballRepresentation.setPosition(b.position.x, b.position.y);
			window.draw(ballRepresentation, rs);
        }

		window.display();

        iterations++;
    }

    return 0;
}
