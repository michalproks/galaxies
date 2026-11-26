#include "EconomyChart.h"

#include <catch2/catch_test_macros.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

TEST_CASE("Economy chart generates ascending ticks for positive and negative domains") {
    std::vector<double> const ticks = Client::generateChartTicks({-8.0, 8.0}, {5, std::nullopt});
    REQUIRE(ticks.size() >= 3);
    CHECK(ticks.front() <= -8.0);
    CHECK(ticks.back() >= 8.0);
    CHECK(std::is_sorted(ticks.begin(), ticks.end()));

    std::vector<double> const zeroTicks = Client::generateChartTicks({0.0, 0.0}, {5, std::nullopt});
    CHECK(zeroTicks.empty());
}

TEST_CASE("Economy chart formats engineering values and units") {
    CHECK(Client::formatChartValue(0.0, Client::ChartUnit::CREDITS) == "0 cc");
    CHECK(Client::formatChartValue(1.0, Client::ChartUnit::CREDITS) == "1 cc");
    CHECK(Client::formatChartValue(999.0, Client::ChartUnit::CREDITS) == "999 cc");
    CHECK(Client::formatChartValue(1000.0, Client::ChartUnit::CREDITS) == "1.00k cc");
    CHECK(Client::formatChartValue(1.0e6, Client::ChartUnit::CREDITS) == "1.00M cc");
    CHECK(Client::formatChartValue(1.0e9, Client::ChartUnit::CREDITS) == "1.00B cc");
    CHECK(Client::formatChartValue(-1250.0, Client::ChartUnit::PERSON_HOURS) == "-1.25k person-hours");
    CHECK(Client::formatChartValue(2.5, Client::ChartUnit::TERAWATTS) == "2.50 TW");
    CHECK(Client::formatChartValue(std::numeric_limits<double>::infinity(), Client::ChartUnit::NONE) == "—");
}

TEST_CASE("Economy chart gives constant and empty data a finite y-scale") {
    std::array<Client::ChartSeries, 1> const constantSeries{
        Client::ChartSeries{"population", Client::ChartUnit::PEOPLE, {{4, 10.0}}, std::nullopt},
    };
    Client::ChartAxisScale const constantScale = Client::calculateChartYAxis(constantSeries);
    CHECK(constantScale.minimum < 10.0);
    CHECK(constantScale.maximum > 10.0);
    CHECK(constantScale.step > 0.0);
    CHECK(constantScale.ticks.size() >= 2);

    std::array<Client::ChartSeries, 1> const emptySeries{
        Client::ChartSeries{"empty", Client::ChartUnit::NONE, {}, std::nullopt},
    };
    Client::ChartAxisScale const emptyScale = Client::calculateChartYAxis(emptySeries);
    CHECK(emptyScale.minimum < emptyScale.maximum);
    CHECK(emptyScale.ticks.size() >= 2);

    std::array<Client::ChartSeries, 1> const zeroSeries{
        Client::ChartSeries{"zero", Client::ChartUnit::NONE, {{0, 0.0}, {1, 0.0}}, std::nullopt},
    };
    Client::ChartAxisScale const zeroScale = Client::calculateChartYAxis(zeroSeries);
    CHECK(zeroScale.minimum < 0.0);
    CHECK(zeroScale.maximum > 0.0);

    std::array<Client::ChartSeries, 1> const windowedSeries{
        Client::ChartSeries{"window", Client::ChartUnit::NONE, {{0, 100000.0}, {10, 4.0}, {11, 6.0}}, std::nullopt},
    };
    Client::ChartAxisScale const windowedScale = Client::calculateChartYAxis(windowedSeries, {5, {{10, 11}}});
    CHECK(windowedScale.maximum < 100.0);
}

TEST_CASE("Economy chart chooses the nearest point across irregular turns") {
    std::array<Client::ChartPoint, 3> const points{{{2, 20.0}, {8, 80.0}, {12, 120.0}}};
    std::optional<Client::ChartPoint> const earlier = Client::nearestChartPoint(points, {5});
    REQUIRE(earlier.has_value());
    Client::ChartPoint const earlierPoint = earlier.value_or(Client::ChartPoint{});
    CHECK(earlierPoint.turn == 2);
    std::optional<Client::ChartPoint> const later = Client::nearestChartPoint(points, {10});
    REQUIRE(later.has_value());
    Client::ChartPoint const laterPoint = later.value_or(Client::ChartPoint{});
    CHECK(laterPoint.turn == 8);
    CHECK(!(Client::nearestChartPoint({}, {4}).has_value()));
}

TEST_CASE("Economy chart clamps scrub turns and hit-tests chart rectangles") {
    Client::ChartTurnRange const range{10, 20};
    CHECK(Client::clampChartScrubTurn(3, range) == 10);
    CHECK(Client::clampChartScrubTurn(30, range) == 20);
    CHECK(Client::clampChartScrubTurn(15, range) == 15);

    Client::ChartRect const rectangle{100.0, 50.0, 300.0, 150.0};
    CHECK(Client::hitTestChart(rectangle, {100.0, 50.0}));
    CHECK(!(Client::hitTestChart(rectangle, {301.0, 100.0})));
    CHECK(Client::chartTurnAtPosition(rectangle, {100.0, 75.0}, range) == 10);
    CHECK(Client::chartTurnAtPosition(rectangle, {300.0, 75.0}, range) == 20);
    CHECK(!(Client::chartTurnAtPosition(rectangle, {301.0, 75.0}, range).has_value()));
}

TEST_CASE("Economy chart renders a headless ImGui frame") {
    ImGuiContext *const context = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = {800.0F, 600.0F};
    io.DeltaTime = 1.0F / 60.0F;
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    REQUIRE(io.Fonts->Build());

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0.0F, 0.0F});
    ImGui::SetNextWindowSize({700.0F, 500.0F});
    ImGui::Begin("Economy chart test");
    Client::EconomyChart chart;
    std::array<Client::ChartSeries, 2> const series{
        Client::ChartSeries{"credits", Client::ChartUnit::CREDITS, {{1, 1500.0}, {2, 2200.0}}, std::nullopt},
        Client::ChartSeries{"reserves", Client::ChartUnit::CREDITS, {{1, 500.0}, {2, 800.0}}, std::nullopt},
    };
    chart.setScrubTurn(2);
    chart.draw("test-chart", series);
    ImGui::End();
    ImGui::Render();
    REQUIRE(ImGui::GetDrawData() != nullptr);
    CHECK(ImGui::GetDrawData()->CmdListsCount > 0);
    ImGui::DestroyContext(context);
}
