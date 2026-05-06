/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "RideSetAppearanceAction.h"

#include "../../Cheats.h"
#include "../../Context.h"
#include "../../Diagnostic.h"
#include "../../core/MemoryStream.h"
#include "../../drawing/Drawing.h"
#include "../../localisation/StringIds.h"
#include "../../ride/Ride.h"
#include "../../ride/RideManager.hpp"
#include "../../ui/WindowManager.h"
#include "../../world/Map.h"
#include "../../world/Park.h"

namespace OpenRCT2::GameActions
{
    RideSetAppearanceAction::RideSetAppearanceAction(
        RideId rideIndex, RideSetAppearanceType type, uint16_t value, uint32_t index)
        : _rideIndex(rideIndex)
        , _type(type)
        , _value(value)
        , _index(index)
    {
    }

    void RideSetAppearanceAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("ride", _rideIndex);
        visitor.Visit("type", _type);
        visitor.Visit("value", _value);
        visitor.Visit("index", _index);
    }

    uint16_t RideSetAppearanceAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags() | Flags::AllowWhilePaused;
    }

    void RideSetAppearanceAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_rideIndex) << DS_TAG(_type) << DS_TAG(_value) << DS_TAG(_index);
    }

    Result RideSetAppearanceAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto ride = GetRide(_rideIndex);
        if (ride == nullptr)
        {
            LOG_ERROR("Ride not found for rideIndex %u", _rideIndex.ToUnderlying());
            return Result(Status::invalidParameters, STR_ERR_INVALID_PARAMETER, STR_ERR_RIDE_NOT_FOUND);
        }

        switch (_type)
        {
            case RideSetAppearanceType::TrackColourMain:
            case RideSetAppearanceType::TrackColourAdditional:
            case RideSetAppearanceType::TrackColourSupports:
                if (_index >= std::size(ride->trackColours))
                {
                    LOG_ERROR("Invalid track colour %u", _index);
                    return Result(Status::invalidParameters, STR_ERR_INVALID_PARAMETER, STR_ERR_INVALID_COLOUR);
                }
                break;
            case RideSetAppearanceType::VehicleColourBody:
            case RideSetAppearanceType::VehicleColourTrim:
            case RideSetAppearanceType::VehicleColourTertiary:
                if (_index >= std::size(ride->vehicleColours))
                {
                    LOG_ERROR("Invalid vehicle colour %u", _index);
                    return Result(Status::invalidParameters, STR_ERR_INVALID_PARAMETER, STR_ERR_INVALID_COLOUR);
                }
                break;
            case RideSetAppearanceType::VehicleColourScheme:
            case RideSetAppearanceType::EntranceStyle:
            case RideSetAppearanceType::SellingItemColourMode:
                break;
            default:
                LOG_ERROR("Invalid ride appearance type %u", _type);
                return Result(Status::invalidParameters, STR_ERR_INVALID_PARAMETER, STR_ERR_VALUE_OUT_OF_RANGE);
        }

        return Result();
    }

    Result RideSetAppearanceAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto ride = GetRide(_rideIndex);
        if (ride == nullptr)
        {
            LOG_ERROR("Ride not found for rideIndex %u", _rideIndex.ToUnderlying());
            return Result(Status::invalidParameters, STR_ERR_INVALID_PARAMETER, STR_ERR_RIDE_NOT_FOUND);
        }

        switch (_type)
        {
            case RideSetAppearanceType::TrackColourMain:
                ride->trackColours[_index].main = static_cast<Drawing::Colour>(_value);
                if (ride->hasRecolourableShopItems() && ride->flags.has(RideFlag::commonShopColours))
                {
                    RideSetCommonShopItemColour(ride, gameState);
                }
                GfxInvalidateScreen();
                break;
            case RideSetAppearanceType::TrackColourAdditional:
                ride->trackColours[_index].additional = static_cast<Drawing::Colour>(_value);
                GfxInvalidateScreen();
                break;
            case RideSetAppearanceType::TrackColourSupports:
                ride->trackColours[_index].supports = static_cast<Drawing::Colour>(_value);
                GfxInvalidateScreen();
                break;
            case RideSetAppearanceType::VehicleColourBody:
                ride->vehicleColours[_index].Body = static_cast<Drawing::Colour>(_value);
                RideUpdateVehicleColours(*ride);
                break;
            case RideSetAppearanceType::VehicleColourTrim:
                ride->vehicleColours[_index].Trim = static_cast<Drawing::Colour>(_value);
                RideUpdateVehicleColours(*ride);
                break;
            case RideSetAppearanceType::VehicleColourTertiary:
                ride->vehicleColours[_index].Tertiary = static_cast<Drawing::Colour>(_value);
                RideUpdateVehicleColours(*ride);
                break;
            case RideSetAppearanceType::VehicleColourScheme:
                ride->vehicleColourSettings = static_cast<VehicleColourSettings>(_value);
                for (uint32_t i = 1; i < std::size(ride->vehicleColours); i++)
                {
                    ride->vehicleColours[i] = ride->vehicleColours[0];
                }
                RideUpdateVehicleColours(*ride);
                break;
            case RideSetAppearanceType::EntranceStyle:
                ride->entranceStyle = _value;
                GfxInvalidateScreen();
                break;
            case RideSetAppearanceType::SellingItemColourMode:
                ShopItemColorMode colorMode = static_cast<ShopItemColorMode>(_value);
                switch (colorMode)
                {
                    case ShopItemColorMode::individual:
                        ride->flags.set(RideFlag::randomShopColours, false);
                        ride->flags.set(RideFlag::commonShopColours, false);
                        break;

                    case ShopItemColorMode::random:
                        ride->flags.set(RideFlag::randomShopColours, true);
                        ride->flags.set(RideFlag::commonShopColours, false);
                        break;

                    case ShopItemColorMode::common:
                        ride->flags.set(RideFlag::randomShopColours, false);
                        ride->flags.set(RideFlag::commonShopColours, true);
                        RideSetCommonShopItemColour(ride, gameState);
                        break;
                }
                GfxInvalidateScreen();
                break;
        }

        auto* windowMgr = Ui::GetWindowManager();
        windowMgr->InvalidateByNumber(WindowClass::ride, _rideIndex.ToUnderlying());

        auto res = Result();
        if (!ride->overallView.IsNull())
        {
            auto location = ride->overallView.ToTileCentre();
            res.position = { location, TileElementHeight(location) };
        }

        return res;
    }

    void RideSetAppearanceAction::RideSetCommonShopItemColour(Ride* ride, OpenRCT2::GameState_t& gameState) const
    {
        const auto* rideEntry = ride->getRideEntry();
        if (rideEntry != nullptr)
        {
            for (size_t itemIndex = 0; itemIndex < std::size(rideEntry->shop_item); ++itemIndex)
            {
                for (auto& otherRide : RideManager(gameState))
                {
                    if (!otherRide.hasRecolourableShopItems())
                        continue;

                    bool invalidate = false;
                    auto* otherRideEntry = GetRideEntryByIndex(otherRide.subtype);

                    const ShopItem currentItem = rideEntry->shop_item[itemIndex];
                    const Drawing::Colour colour = ride->trackColours[_index].main;

                    if (otherRideEntry != nullptr && otherRideEntry->shop_item[0] == currentItem)
                    {
                        if (otherRide.trackColours[0].main != colour)
                        {
                            otherRide.trackColours[0].main = colour;
                            otherRide.flags.set(RideFlag::commonShopColours);
                            otherRide.flags.unset(RideFlag::randomShopColours);
                            invalidate = true;
                        }
                    }

                    if (invalidate)
                    {
                        auto* windowMgr = Ui::GetWindowManager();
                        windowMgr->InvalidateByNumber(WindowClass::ride, otherRide.id.ToUnderlying());
                    }
                }
            }
        }
    }
} // namespace OpenRCT2::GameActions
