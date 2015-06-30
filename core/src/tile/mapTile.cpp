#include "mapTile.h"

#include "style/style.h"
#include "view/view.h"
#include "util/tileID.h"
#include "util/vboMesh.h"
#include "util/shaderProgram.h"
#include "text/fontContext.h"
#include "labels/labels.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


MapTile::MapTile(TileID _id, const MapProjection& _projection)
    : m_id(_id),
      m_projection(&_projection) {

    BoundingBox bounds(_projection.TileBounds(_id));

    m_scale = 0.5 * bounds.width();
    m_inverseScale = 1.0/m_scale;
    
    m_tileOrigin = bounds.center();
    // negative y coordinate: to change from y down to y up (tile system has y down and gl context we use has y up).
    m_tileOrigin.y *= -1.0;

    // Init model matrix to size of tile
    m_modelMatrix = glm::scale(glm::mat4(1.0), glm::vec3(m_scale));
}

MapTile::~MapTile() {

}

void MapTile::addGeometry(const Style& _style, std::shared_ptr<VboMesh> _mesh) {

    m_geometry[_style.getName()] = std::move(_mesh); // Move-construct a unique_ptr at the value associated with the given style

}

void MapTile::setTextBuffer(const Style& _style, std::shared_ptr<TextBuffer> _buffer) {

    m_buffers[_style.getName()] = _buffer;
}

std::shared_ptr<TextBuffer> MapTile::getTextBuffer(const Style& _style) const {
    auto it = m_buffers.find(_style.getName());

    if (it != m_buffers.end()) {
        return it->second;
    }

    return nullptr;
}

void MapTile::update(float _dt, const View& _view) {

    // Apply tile-view translation to the model matrix
    const auto& viewOrigin = _view.getPosition();
    m_modelMatrix[3][0] = m_tileOrigin.x - viewOrigin.x;
    m_modelMatrix[3][1] = m_tileOrigin.y - viewOrigin.y;
    m_modelMatrix[3][2] = -viewOrigin.z;

}

void MapTile::updateLabels(float _dt, const Style& _style, const View& _view) {
    glm::mat4 mvp = _view.getViewProjectionMatrix() * m_modelMatrix;
    glm::vec2 screenSize = glm::vec2(_view.getWidth(), _view.getHeight());
    
    for (auto& label : m_labels[_style.getName()]) {
        label->update(mvp, screenSize, _dt);
    }
}

void MapTile::pushLabelTransforms(const Style& _style, std::shared_ptr<Labels> _labels) {
    auto it = m_buffers.find(_style.getName());
    
    if (it == m_buffers.end()) {
        return;
    }

    auto textBuffer = it->second;
    
    if (textBuffer->hasData()) {
        auto ftContext = _labels->getFontContext();

        // FIXME : locking during sprite updates
        ftContext->lock();
        ftContext->useBuffer(textBuffer);
        
        for (auto& label : m_labels[_style.getName()]) {
            label->pushTransform();
        }
        
        textBuffer->pushBuffer();
        ftContext->unlock();
    }
    
}

void MapTile::draw(const Style& _style, const View& _view) {

    const std::shared_ptr<VboMesh>& styleMesh = m_geometry[_style.getName()];
    
    if (styleMesh) {
        
        std::shared_ptr<ShaderProgram> shader = _style.getShaderProgram();

        glm::mat4 modelViewMatrix = _view.getViewMatrix() * m_modelMatrix;
        glm::mat4 modelViewProjMatrix = _view.getViewProjectionMatrix() * m_modelMatrix;
        
        shader->setUniformMatrix4f("u_modelView", glm::value_ptr(modelViewMatrix));
        shader->setUniformMatrix4f("u_modelViewProj", glm::value_ptr(modelViewProjMatrix));
        shader->setUniformMatrix3f("u_normalMatrix", glm::value_ptr(_view.getNormalMatrix()));

        // Set the tile zoom level, using the sign to indicate whether the tile is a proxy
        shader->setUniformf("u_tile_zoom", m_proxyCounter > 0 ? -m_id.z : m_id.z);

        styleMesh->draw(shader);
    }
}

std::shared_ptr<VboMesh>& MapTile::getGeometry(const Style& _style) {
    return m_geometry.at(_style.getName());
}

void MapTile::addLabel(const std::string& _styleName, std::shared_ptr<Label> _label) {
    m_labels[_styleName].push_back(std::move(_label));
}
