//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

internal readonly record struct TownRoutePoint(float X, float Z, float Wait = 0);

internal static class VideoTownRoutes
{
    // Routes stay in world metres on the pavement; only the marked crossings enter the road.
    private static readonly TownRoutePoint[] Street =
    [
        new(-37, -5.2f), new(-20, -5.2f), new(0, -5.2f), new(20, -5.2f),
        new(35, -5.2f, 3.5f), new(38, -5.2f), new(38, -4.1f, 1.3f),
        new(38, 4.1f), new(38, 5.2f), new(20, 5.2f), new(0, 5.2f),
        new(-20, 5.2f), new(-37, 5.2f), new(-37, 4.1f, 1.3f), new(-37, -4.1f)
    ];

    private static readonly TownRoutePoint[] Cinema =
    [
        new(-6, 10.2f), new(0, 10.2f), new(6, 10.2f), new(6, 20.8f),
        new(3.5f, 20.8f), new(0, 20.8f, 4), new(-6, 20.8f)
    ];

    private static readonly TownRoutePoint[] Porches =
    [
        new(-23.75f, -6), new(-23.75f, -10.5f), new(-23.75f, -12.2f, 1),
        new(-23.75f, -10.5f), new(-23.75f, -6), new(-10, -5.2f),
        new(1.25f, -5.2f), new(1.25f, -11.2f), new(1.25f, -12.2f, 1),
        new(1.25f, -5.2f)
    ];

    public static TownRoutePoint[] Get(int route) => route switch
    {
        1 => Cinema,
        2 => Porches,
        _ => Street
    };
}
