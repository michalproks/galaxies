#include "EconomyChart.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>

namespace Client {
namespace {

    constexpr std::array<ChartColor, 6> SERIES_PALETTE{
        ChartColor{82, 176, 255},
        ChartColor{255, 190, 75},
        ChartColor{107, 214, 139},
        ChartColor{239, 112, 132},
        ChartColor{190, 145, 255},
        ChartColor{76, 214, 207},
    };
    constexpr double PLOT_LEFT_MARGIN = 70.0;
    constexpr double PLOT_RIGHT_MARGIN = 12.0;
    constexpr double PLOT_TOP_MARGIN = 25.0;
    constexpr double PLOT_BOTTOM_MARGIN = 48.0;
    constexpr double GRID_THICKNESS = 1.0;
    constexpr double SERIES_THICKNESS = 2.0;
    constexpr double SCRUB_THICKNESS = 2.0;

    [[nodiscard]] double niceStep(double const rawStep) {
        double const exponent = std::floor(std::log10(rawStep));
        double const magnitude = std::pow(10.0, exponent);
        double const normalized = rawStep / magnitude;
        double const niceNormalized =
            normalized <= 1.0 ? 1.0 : (normalized <= 2.0 ? 2.0 : (normalized <= 5.0 ? 5.0 : 10.0));
        return niceNormalized * magnitude;
    }

    [[nodiscard]] std::vector<double> ticksForScale(ChartNumericRange range, double step) {
        double const minimum = range.minimum;
        double const maximum = range.maximum;
        std::vector<double> ticks;
        if (!std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(step) || step <= 0.0) {
            return ticks;
        }
        double const rawCount = std::floor((maximum - minimum) / step) + 1.0;
        if (!std::isfinite(rawCount) || rawCount > 1000.0) {
            return ticks;
        }
        auto const count = static_cast<std::size_t>(rawCount);
        ticks.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            ticks.push_back(minimum + static_cast<double>(index) * step);
        }
        return ticks;
    }

    [[nodiscard]] ChartAxisScale scaleForRange(ChartNumericRange range, std::size_t desiredTickCount) {
        double minimum = range.minimum;
        double maximum = range.maximum;
        if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
            minimum = 0.0;
            maximum = 0.0;
        }
        if (minimum > maximum) {
            std::swap(minimum, maximum);
        }
        if (minimum == maximum) {
            double const padding = std::max(std::abs(minimum) * 0.1, 1.0);
            minimum -= padding;
            maximum += padding;
        } else {
            double const padding = (maximum - minimum) * 0.05;
            minimum -= padding;
            maximum += padding;
        }

        std::size_t const tickCount = std::max<std::size_t>(desiredTickCount, 2);
        double const step = niceStep((maximum - minimum) / static_cast<double>(tickCount - 1));
        double const roundedMinimum = std::floor(minimum / step) * step;
        double const roundedMaximum = std::ceil(maximum / step) * step;
        ChartAxisScale scale;
        scale.minimum = roundedMinimum;
        scale.maximum = roundedMaximum > roundedMinimum ? roundedMaximum : roundedMinimum + step;
        scale.step = step;
        scale.ticks = ticksForScale({scale.minimum, scale.maximum}, step);
        return scale;
    }

    [[nodiscard]] std::optional<ChartTurnRange> dataTurnRange(std::span<ChartSeries const> series) {
        std::optional<ChartTurnRange> range;
        for (ChartSeries const &entry : series) {
            for (ChartPoint const &point : entry.points) {
                if (!std::isfinite(point.value)) {
                    continue;
                }
                if (!range.has_value()) {
                    range = ChartTurnRange{point.turn, point.turn};
                } else {
                    range->firstTurn = std::min(range->firstTurn, point.turn);
                    range->lastTurn = std::max(range->lastTurn, point.turn);
                }
            }
        }
        if (range.has_value() && range->firstTurn == range->lastTurn) {
            if (range->firstTurn > std::numeric_limits<std::int64_t>::min()) {
                --range->firstTurn;
            }
            if (range->lastTurn < std::numeric_limits<std::int64_t>::max()) {
                ++range->lastTurn;
            }
        }
        return range;
    }

    [[nodiscard]] std::uint32_t colorValue(ChartColor color) {
        return static_cast<ImU32>(ImColor(color.red, color.green, color.blue, 255));
    }

    [[nodiscard]] ImU32 packColor(ChartColor color, std::uint8_t alpha) {
        return static_cast<ImU32>(ImColor(color.red, color.green, color.blue, alpha));
    }

    [[nodiscard]] ChartColor seriesColor(ChartSeries const &series, std::size_t index) {
        return series.color.value_or(SERIES_PALETTE[index % SERIES_PALETTE.size()]);
    }

    [[nodiscard]] double xForTurn(ChartRect const &plot, ChartTurnRange range, std::int64_t turn) {
        if (range.firstTurn == range.lastTurn) {
            return (plot.left + plot.right) / 2.0;
        }
        long double const offset = static_cast<long double>(turn) - static_cast<long double>(range.firstTurn);
        long double const span = static_cast<long double>(range.lastTurn) - static_cast<long double>(range.firstTurn);
        return plot.left + static_cast<double>(offset / span) * (plot.right - plot.left);
    }

    [[nodiscard]] double yForValue(ChartRect const &plot, ChartAxisScale const &scale, double value) {
        double const fraction = (value - scale.minimum) / (scale.maximum - scale.minimum);
        return plot.bottom - fraction * (plot.bottom - plot.top);
    }

    [[nodiscard]] std::int64_t turnAtX(ChartRect const &plot, ChartTurnRange range, double x) {
        if (range.firstTurn == range.lastTurn || plot.right <= plot.left) {
            return range.firstTurn;
        }
        double const fraction = std::clamp((x - plot.left) / (plot.right - plot.left), 0.0, 1.0);
        long double const span = static_cast<long double>(range.lastTurn) - static_cast<long double>(range.firstTurn);
        long double const turn = static_cast<long double>(range.firstTurn) + std::round(fraction * span);
        return clampChartScrubTurn(static_cast<std::int64_t>(turn), range);
    }

    [[nodiscard]] std::string turnLabel(std::int64_t turn) {
        return std::to_string(turn);
    }

} // namespace

std::vector<double> generateChartTicks(ChartNumericRange const range, ChartScaleOptions const options) {
    double const minimum = range.minimum;
    double const maximum = range.maximum;
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum == maximum) {
        return {};
    }
    double low = std::min(minimum, maximum);
    double high = std::max(minimum, maximum);
    double const padding = (high - low) * 0.05;
    low -= padding;
    high += padding;
    std::size_t const tickCount = std::max<std::size_t>(options.desiredTickCount, 2);
    double const step = niceStep((high - low) / static_cast<double>(tickCount - 1));
    double const roundedMinimum = std::floor(low / step) * step;
    double const roundedMaximum = std::ceil(high / step) * step;
    return ticksForScale({roundedMinimum, roundedMaximum > roundedMinimum ? roundedMaximum : roundedMinimum + step},
                         step);
}

ChartAxisScale calculateChartYAxis(std::span<ChartSeries const> const series, ChartScaleOptions const options) {
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    std::optional<ChartTurnRange> const visibleRange =
        options.turnRange.has_value() ? std::optional<ChartTurnRange>{ChartTurnRange{
                                            std::min(options.turnRange->firstTurn, options.turnRange->lastTurn),
                                            std::max(options.turnRange->firstTurn, options.turnRange->lastTurn),
                                        }}
                                      : std::nullopt;
    for (ChartSeries const &entry : series) {
        for (ChartPoint const &point : entry.points) {
            if (visibleRange.has_value() &&
                (point.turn < visibleRange->firstTurn || point.turn > visibleRange->lastTurn)) {
                continue;
            }
            if (std::isfinite(point.value)) {
                minimum = std::min(minimum, point.value);
                maximum = std::max(maximum, point.value);
            }
        }
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
        minimum = 0.0;
        maximum = 0.0;
    }
    return scaleForRange({minimum, maximum}, options.desiredTickCount);
}

std::string formatChartValue(double const value, ChartUnit const unit) {
    if (!std::isfinite(value)) {
        return "—";
    }
    double magnitude = std::abs(value);
    std::string_view prefix;
    if (magnitude >= 1.0e9) {
        magnitude /= 1.0e9;
        prefix = "B";
    } else if (magnitude >= 1.0e6) {
        magnitude /= 1.0e6;
        prefix = "M";
    } else if (magnitude >= 1.0e3) {
        magnitude /= 1.0e3;
        prefix = "k";
    } else {
        magnitude = std::abs(value);
    }
    double const signedMagnitude = std::signbit(value) ? -magnitude : magnitude;
    char number[64]{};
    int precision{};
    if (!(prefix.empty() && magnitude == std::floor(magnitude)) && magnitude != 0.0) {
        precision = magnitude < 10.0 ? 2 : (magnitude < 100.0 && magnitude != std::floor(magnitude) ? 1 : 0);
    }
    switch (precision) {
    case 1:
        std::snprintf(number, sizeof(number), "%.1f", signedMagnitude);
        break;
    case 2:
        std::snprintf(number, sizeof(number), "%.2f", signedMagnitude);
        break;
    default:
        std::snprintf(number, sizeof(number), "%.0f", signedMagnitude);
        break;
    }

    std::string suffix(prefix);
    switch (unit) {
    case ChartUnit::NONE:
        break;
    case ChartUnit::CREDITS:
        suffix += " cc";
        break;
    case ChartUnit::PERSON_HOURS:
        suffix += " person-hours";
        break;
    case ChartUnit::TERAWATTS:
        suffix += " TW";
        break;
    case ChartUnit::TONS:
        suffix += " t";
        break;
    case ChartUnit::PEOPLE:
        suffix += " people";
        break;
    case ChartUnit::COUNT:
        break;
    }
    return std::string(number) + suffix;
}

std::optional<ChartPoint> nearestChartPoint(std::span<ChartPoint const> const points, ChartPointQuery const query) {
    std::optional<ChartPoint> nearest;
    long double nearestDistance = std::numeric_limits<long double>::infinity();
    for (ChartPoint const &point : points) {
        if (!std::isfinite(point.value)) {
            continue;
        }
        long double const distance =
            std::abs(static_cast<long double>(point.turn) - static_cast<long double>(query.turn));
        if (distance < nearestDistance) {
            nearest = point;
            nearestDistance = distance;
        }
    }
    return nearest;
}

std::int64_t clampChartScrubTurn(std::int64_t const turn, ChartTurnRange const range) {
    return std::clamp(turn, std::min(range.firstTurn, range.lastTurn), std::max(range.firstTurn, range.lastTurn));
}

bool hitTestChart(ChartRect const rectangle, ChartPosition const position) {
    return std::isfinite(position.x) && std::isfinite(position.y) && position.x >= rectangle.left &&
           position.x <= rectangle.right && position.y >= rectangle.top && position.y <= rectangle.bottom;
}

std::optional<std::int64_t>
chartTurnAtPosition(ChartRect const rectangle, ChartPosition const position, ChartTurnRange const range) {
    ChartTurnRange const normalizedRange{
        std::min(range.firstTurn, range.lastTurn),
        std::max(range.firstTurn, range.lastTurn),
    };
    if (!hitTestChart(rectangle, position)) {
        return std::nullopt;
    }
    return turnAtX(rectangle, normalizedRange, position.x);
}

void EconomyChart::draw(std::string_view const id,
                        std::span<ChartSeries const> const series,
                        EconomyChartOptions const &options) {
    float const widgetHeight = std::max(options.height, 120.0F);
    std::string const widgetId(id);
    if (!options.title.empty()) {
        ImGui::TextUnformatted(options.title.data(), options.title.data() + options.title.size());
    }

    ImVec2 const widgetSize{std::max(ImGui::GetContentRegionAvail().x, 1.0F), widgetHeight};
    ImGui::InvisibleButton(widgetId.c_str(), widgetSize);
    ImVec2 const widgetMin = ImGui::GetItemRectMin();
    ImVec2 const widgetMax = ImGui::GetItemRectMax();
    ImDrawList *const drawList = ImGui::GetWindowDrawList();
    ImVec2 const plotMin{widgetMin.x + static_cast<float>(PLOT_LEFT_MARGIN),
                         widgetMin.y + static_cast<float>(PLOT_TOP_MARGIN)};
    ImVec2 const plotMax{widgetMax.x - static_cast<float>(PLOT_RIGHT_MARGIN),
                         widgetMax.y - static_cast<float>(PLOT_BOTTOM_MARGIN)};
    ChartRect const plot{
        static_cast<double>(plotMin.x),
        static_cast<double>(plotMin.y),
        static_cast<double>(std::max(plotMax.x, plotMin.x + 1.0F)),
        static_cast<double>(std::max(plotMax.y, plotMin.y + 1.0F)),
    };
    std::optional<ChartTurnRange> const rawDataRange = dataTurnRange(series);
    ChartTurnRange range = options.turnRange.value_or(rawDataRange.value_or(ChartTurnRange{0, 1}));
    if (range.firstTurn > range.lastTurn) {
        std::swap(range.firstTurn, range.lastTurn);
    }
    if (range.firstTurn == range.lastTurn) {
        if (range.firstTurn > std::numeric_limits<std::int64_t>::min()) {
            --range.firstTurn;
        }
        if (range.lastTurn < std::numeric_limits<std::int64_t>::max()) {
            ++range.lastTurn;
        }
    }
    ChartAxisScale const yScale = calculateChartYAxis(series, {5, range});
    if (mScrubTurn.has_value()) {
        mScrubTurn = clampChartScrubTurn(*mScrubTurn, range);
    }

    ImU32 const TEXT_COLOR = packColor({210, 210, 210}, 255);
    ImU32 const GRID_COLOR = packColor({80, 80, 80}, 190);
    ImU32 const PLOT_BORDER_COLOR = packColor({140, 140, 140}, 255);
    ImU32 const SCRUB_COLOR = packColor({255, 255, 255}, 210);
    ImGui::PushClipRect(widgetMin, widgetMax, true);
    drawList->AddRectFilled(widgetMin, widgetMax, packColor({24, 27, 33}, 255), 3.0F);
    drawList->AddRect(plotMin, plotMax, PLOT_BORDER_COLOR);

    for (double const tick : yScale.ticks) {
        double const y = yForValue(plot, yScale, tick);
        drawList->AddLine({plotMin.x, static_cast<float>(y)},
                          {plotMax.x, static_cast<float>(y)},
                          GRID_COLOR,
                          static_cast<float>(GRID_THICKNESS));
        std::string const label = formatChartValue(tick, series.empty() ? ChartUnit::NONE : series.front().unit);
        drawList->AddText(
            {widgetMin.x + 4.0F, static_cast<float>(y) - ImGui::GetTextLineHeight() / 2.0F}, TEXT_COLOR, label.c_str());
    }

    constexpr std::size_t X_TICK_COUNT = 5;
    for (std::size_t index = 0; index < X_TICK_COUNT; ++index) {
        double const fraction = static_cast<double>(index) / static_cast<double>(X_TICK_COUNT - 1);
        long double const turnSpan =
            static_cast<long double>(range.lastTurn) - static_cast<long double>(range.firstTurn);
        std::int64_t const tickTurn =
            static_cast<std::int64_t>(static_cast<long double>(range.firstTurn) + std::round(fraction * turnSpan));
        double const x = xForTurn(plot, range, tickTurn);
        drawList->AddLine({static_cast<float>(x), plotMin.y},
                          {static_cast<float>(x), plotMax.y},
                          GRID_COLOR,
                          static_cast<float>(GRID_THICKNESS));
        std::string const label = turnLabel(tickTurn);
        ImVec2 const labelSize = ImGui::CalcTextSize(label.c_str());
        drawList->AddText({static_cast<float>(x) - labelSize.x / 2.0F, plotMax.y + 4.0F}, TEXT_COLOR, label.c_str());
    }

    for (std::size_t seriesIndex = 0; seriesIndex < series.size(); ++seriesIndex) {
        ChartSeries const &entry = series[seriesIndex];
        std::uint32_t const color = colorValue(seriesColor(entry, seriesIndex));
        bool havePrevious{};
        ImVec2 previous{};
        for (ChartPoint const &point : entry.points) {
            if (point.turn < range.firstTurn || point.turn > range.lastTurn || !std::isfinite(point.value)) {
                havePrevious = false;
                continue;
            }
            ImVec2 const current{static_cast<float>(xForTurn(plot, range, point.turn)),
                                 static_cast<float>(yForValue(plot, yScale, point.value))};
            if (havePrevious) {
                drawList->AddLine(previous, current, color, static_cast<float>(SERIES_THICKNESS));
            }
            drawList->AddCircleFilled(current, 3.0F, color);
            previous = current;
            havePrevious = true;
        }
    }

    if (options.showLegend) {
        float legendX = plotMin.x;
        float const legendY = widgetMax.y - 20.0F;
        for (std::size_t index = 0; index < series.size(); ++index) {
            ChartSeries const &entry = series[index];
            std::uint32_t const color = colorValue(seriesColor(entry, index));
            drawList->AddLine({legendX, legendY + 6.0F}, {legendX + 14.0F, legendY + 6.0F}, color, 2.0F);
            legendX += 19.0F;
            drawList->AddText({legendX, legendY}, TEXT_COLOR, entry.label.c_str());
            legendX += ImGui::CalcTextSize(entry.label.c_str()).x + 14.0F;
        }
    }
    if (series.empty() || !rawDataRange.has_value()) {
        constexpr char EMPTY_LABEL[] = "No data";
        ImVec2 const labelSize = ImGui::CalcTextSize(EMPTY_LABEL);
        drawList->AddText({(plotMin.x + plotMax.x - labelSize.x) / 2.0F, (plotMin.y + plotMax.y - labelSize.y) / 2.0F},
                          TEXT_COLOR,
                          EMPTY_LABEL);
    }

    bool const hovered = ImGui::IsItemHovered();
    ImVec2 const mouse = ImGui::GetIO().MousePos;
    ChartPosition const mousePosition{static_cast<double>(mouse.x), static_cast<double>(mouse.y)};
    bool const mouseInPlot = hitTestChart(plot, mousePosition);
    if (options.enableScrubber && rawDataRange.has_value() && hovered && mouseInPlot &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        mScrubTurn = turnAtX(plot, range, mousePosition.x);
        if (options.onScrubTurnChanged) {
            options.onScrubTurnChanged(*mScrubTurn);
        }
    } else if (options.enableScrubber && rawDataRange.has_value() && ImGui::IsItemActive() &&
               ImGui::IsMouseDown(ImGuiMouseButton_Left) && mouseInPlot) {
        std::int64_t const turn = turnAtX(plot, range, mousePosition.x);
        if (!mScrubTurn.has_value() || *mScrubTurn != turn) {
            mScrubTurn = turn;
            if (options.onScrubTurnChanged) {
                options.onScrubTurnChanged(turn);
            }
        }
    }
    if (mScrubTurn.has_value()) {
        double const scrubX = xForTurn(plot, range, *mScrubTurn);
        drawList->AddLine({static_cast<float>(scrubX), plotMin.y},
                          {static_cast<float>(scrubX), plotMax.y},
                          SCRUB_COLOR,
                          static_cast<float>(SCRUB_THICKNESS));
    }

    if (hovered && mouseInPlot && rawDataRange.has_value()) {
        std::int64_t const hoverTurn = turnAtX(plot, range, mousePosition.x);
        double const hoverX = xForTurn(plot, range, hoverTurn);
        drawList->AddLine({static_cast<float>(hoverX), plotMin.y},
                          {static_cast<float>(hoverX), plotMax.y},
                          packColor({255, 255, 255}, 100),
                          1.0F);
        ImGui::BeginTooltip();
        std::string const turnText = "Turn " + std::to_string(hoverTurn);
        ImGui::TextUnformatted(turnText.c_str());
        for (std::size_t index = 0; index < series.size(); ++index) {
            ChartSeries const &entry = series[index];
            std::optional<ChartPoint> const point = nearestChartPoint(entry.points, {hoverTurn});
            ImGui::PushStyleColor(ImGuiCol_Text, colorValue(seriesColor(entry, index)));
            if (point.has_value()) {
                std::string const value = entry.label + ": " + formatChartValue(point->value, entry.unit);
                ImGui::TextUnformatted(value.c_str());
            } else {
                std::string const value = entry.label + ": —";
                ImGui::TextUnformatted(value.c_str());
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndTooltip();
    }
    ImGui::PopClipRect();
}

void EconomyChart::setScrubTurn(std::int64_t const turn) noexcept {
    mScrubTurn = turn;
}

void EconomyChart::clearScrubTurn() noexcept {
    mScrubTurn.reset();
}

std::optional<std::int64_t> EconomyChart::scrubTurn() const noexcept {
    return mScrubTurn;
}

} // namespace Client
