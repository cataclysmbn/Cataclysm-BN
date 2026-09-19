#include "cata_generators.h"

#include "point.h"
#include "rng.h"

class RandomPointGenerator final: public Catch::Generators::IGenerator<point> {
public:
    RandomPointGenerator(int low, int high): engine(rng_get_engine()), dist(low, high) {
        this->next();
    }

    auto get() const -> const point& override { return current_point; } // *NOPAD*

    auto next() -> bool override {
        current_point = point(dist(engine), dist(engine));
        return true;
    }

protected:
    cata_default_random_engine& engine;
    std::uniform_int_distribution<> dist;
    point current_point;
};

class RandomTripointGenerator final: public Catch::Generators::IGenerator<tripoint> {
public:
    RandomTripointGenerator(int low, int high, int zlow, int zhigh)
        : engine(rng_get_engine()),
          xy_dist(low, high),
          z_dist(zlow, zhigh) {
        this->next();
    }

    auto get() const -> const tripoint& override { return current_point; } // *NOPAD*

    auto next() -> bool override {
        current_point = tripoint(xy_dist(engine), xy_dist(engine), z_dist(engine));
        return true;
    }

protected:
    cata_default_random_engine& engine;
    std::uniform_int_distribution<> xy_dist;
    std::uniform_int_distribution<> z_dist;
    tripoint current_point;
};

auto random_points(int low, int high) -> Catch::Generators::GeneratorWrapper<point> {
    return Catch::Generators::GeneratorWrapper<point>(
        std::make_unique<RandomPointGenerator>(low, high));
}

auto random_tripoints(int low, int high, int zlow, int zhigh)
    -> Catch::Generators::GeneratorWrapper<tripoint> {
    return Catch::Generators::GeneratorWrapper<tripoint>(
        std::make_unique<RandomTripointGenerator>(low, high, zlow, zhigh));
}
