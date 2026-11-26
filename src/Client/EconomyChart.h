#ifndef GALAXIES_CLIENT_ECONOMY_CHART_H
#define GALAXIES_CLIENT_ECONOMY_CHART_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Client {

/** Unit used to format chart-axis ticks and per-series hover values. */
enum class ChartUnit : std::uint8_t { NONE, CREDITS, PERSON_HOURS, TERAWATTS, TONS, PEOPLE, COUNT };

/** One discrete value on a series, keyed by its completed economy turn. */
struct ChartPoint {
    std::int64_t turn{};
    double value{};
};

/** RGB color for a series; unset series colors use the widget's color palette. */
struct ChartColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

/** Plain chart data, independent of simulation record types; points are expected in ascending turn order. */
struct ChartSeries {
    std::string label;
    ChartUnit unit{ChartUnit::NONE};
    std::vector<ChartPoint> points;
    std::optional<ChartColor> color;
};

/** Inclusive bounds of the visible turn window. */
struct ChartTurnRange {
    std::int64_t firstTurn{};
    std::int64_t lastTurn{};
};

/** Floating-point rectangle used by deterministic chart hit-testing. */
struct ChartRect {
    double left{};
    double top{};
    double right{};
    double bottom{};
};

/** Screen-space position used by deterministic chart hit-testing. */
struct ChartPosition {
    double x{};
    double y{};
};

/** A numeric axis domain and its evenly spaced, readable tick locations. */
struct ChartAxisScale {
    double minimum{};
    double maximum{1.0};
    double step{1.0};
    std::vector<double> ticks;
};

/** Bounds supplied to a numeric chart-axis computation. */
struct ChartNumericRange {
    double minimum{};
    double maximum{};
};

/** Requested density for generated numeric-axis tick marks. */
struct ChartScaleOptions {
    std::size_t desiredTickCount{5};
    std::optional<ChartTurnRange> turnRange;
};

/** Turn to use when looking up the closest point in a series. */
struct ChartPointQuery {
    std::int64_t turn{};
};

/** Creates "nice" ticks for finite, distinct range bounds; equal or non-finite bounds return no ticks. */
[[nodiscard]] std::vector<double> generateChartTicks(ChartNumericRange range, ChartScaleOptions options = {});

/** Computes a shared y-axis scale; empty or all-zero input receives a safe nonzero domain. */
[[nodiscard]] ChartAxisScale calculateChartYAxis(std::span<ChartSeries const> series, ChartScaleOptions options = {});

/** Formats a value with compact engineering prefixes and the requested display unit. */
[[nodiscard]] std::string formatChartValue(double value, ChartUnit unit);

/** Returns the point with minimum absolute turn distance; ties keep the first point. */
[[nodiscard]] std::optional<ChartPoint> nearestChartPoint(std::span<ChartPoint const> points, ChartPointQuery query);

/** Clamps a requested scrub turn to an inclusive visible range. */
[[nodiscard]] std::int64_t clampChartScrubTurn(std::int64_t turn, ChartTurnRange range);

/** Tests whether a finite position lies inside a rectangle, including its boundary. */
[[nodiscard]] bool hitTestChart(ChartRect rectangle, ChartPosition position);

/** Maps an in-rectangle horizontal position to a turn, or returns no value outside the rectangle. */
[[nodiscard]] std::optional<std::int64_t>
chartTurnAtPosition(ChartRect rectangle, ChartPosition position, ChartTurnRange range);

/** Geometry, inclusive rolling-window bounds, and optional interaction behavior for one chart widget. */
struct EconomyChartOptions {
    std::string_view title;
    float height{240.0F};
    std::optional<ChartTurnRange> turnRange;
    bool showLegend{true};
    bool enableScrubber{true};
    /** Called with the clamped turn whenever a click or drag changes the scrub cursor. */
    std::function<void(std::int64_t)> onScrubTurnChanged;
};

/** Stateful ImGui chart renderer with hover inspection and a draggable turn scrubber. */
class EconomyChart {
public:
    /** Draws between NewFrame and Render; id must be unique among sibling charts in the current window. */
    void draw(std::string_view id, std::span<ChartSeries const> series, EconomyChartOptions const &options = {});

    /** Sets the persistent scrub cursor; it is clamped to the visible window on the next draw. */
    void setScrubTurn(std::int64_t turn) noexcept;

    /** Hides and clears the persistent scrub cursor. */
    void clearScrubTurn() noexcept;

    /** Returns the requested persistent cursor, or no value when it has been cleared. */
    [[nodiscard]] std::optional<std::int64_t> scrubTurn() const noexcept;

private:
    std::optional<std::int64_t> mScrubTurn;
};

} // namespace Client

#endif // GALAXIES_CLIENT_ECONOMY_CHART_H
