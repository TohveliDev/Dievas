#pragma once

// Initial Layer
// Used for the runtime application to access specific Application functions
class Layer
{
public:
    virtual ~Layer() = default;

    virtual void onAttach() {}
    virtual void onDetach() {}

    virtual void onUpdate(float deltaTime) {}
    virtual void onUIRender() {}
};