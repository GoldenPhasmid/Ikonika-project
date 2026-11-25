import math

def Path(Vertices, Incoming=None, Outgoing=None):
    return {'c': True, 'v': Vertices, 'i': Incoming or [[0, 0] for Point in Vertices], 'o': Outgoing or [[0, 0] for Point in Vertices]}

def Circle(Point, Enabled=True):
    X, Y, Radius = Point
    Radius = Radius if Enabled else 0
    Handle = Radius * 0.5522847498307936
    return Path([[X + Radius, Y], [X, Y - Radius], [X - Radius, Y], [X, Y + Radius]], [[0, Handle], [Handle, 0], [0, -Handle], [-Handle, 0]], [[0, -Handle], [-Handle, 0], [0, Handle], [Handle, 0]])

def FramePaths(Frame):
    Points = Frame['points']
    Result = []
    for A, B in zip(Points, Points[1:]):
        X, Y = B[0] - A[0], B[1] - A[1]
        Length = max(1e-8, math.hypot(X, Y))
        NormalX, NormalY = -Y / Length, X / Length
        Result.append(Path([[A[0] + NormalX * A[2], A[1] + NormalY * A[2]], [B[0] + NormalX * B[2], B[1] + NormalY * B[2]], [B[0] - NormalX * B[2], B[1] - NormalY * B[2]], [A[0] - NormalX * A[2], A[1] - NormalY * A[2]]]))
    for Index, Point in enumerate(Points):
        Enabled = Frame['start'] == 0 if Index == 0 else Frame['end'] == 0 if Index == len(Points) - 1 else True
        Result.append(Circle(Point, Enabled))
    return Result

def Shapes(Frames, Stroke, Name):
    Samples = [FramePaths(Frame) for Frame in Frames]
    Items = []
    for Index in range(len(Samples[0])):
        Keys = []
        for Frame in range(len(Samples)):
            Keys.append({'t': Frame + 1, 's': [Samples[Frame][Index]], 'e': [Samples[min(len(Samples) - 1, Frame + 1)][Index]], 'o': {'x': 0, 'y': 0}, 'i': {'x': 1, 'y': 1}})
        Keys.append({'t': len(Samples) + 1})
        Items.append({'ty': 'sh', 'nm': 'foreleg-envelope-' + str(Index), 'ks': {'a': 1, 'k': Keys}})
    Items.append({'ty': 'fl', 'c': Stroke['c'], 'o': Stroke['o'], 'r': 1})
    Items.append({'ty': 'tr', 'p': {'a': 0, 'k': [0, 0]}, 'a': {'a': 0, 'k': [0, 0]}, 's': {'a': 0, 'k': [100, 100]}, 'r': {'a': 0, 'k': 0}, 'o': {'a': 0, 'k': 100}})
    return [{'ty': 'gr', 'nm': Name + ' variable width', 'it': Items}]
