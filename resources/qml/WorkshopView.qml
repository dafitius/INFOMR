import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import QtQuick3D.Helpers
import MultimediaRetrieval.Types

Rectangle {

    View3D {
        id: v3d
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        camera: camera

        environment: SceneEnvironment {
            id: env
            backgroundMode: SceneEnvironment.Color
            clearColor: "#002b36"
        }

        Node {
            id: originNode
            position: Qt.vector3d(0, 0, 0)
            rotation: Qt.quaternion(1, 0, 0, 0)
            PerspectiveCamera {
                id: cameraNode
                z: 200
            }
        }

        DirectionalLight {
            id: directionalLight
            color: Qt.rgba(0.4, 0.2, 0.6, 1.0)
            ambientColor: Qt.rgba(0.1, 0.1, 0.1, 1.0)
        }

        PointLight {
            id: pointLight
            position: Qt.vector3d(0, 0, 100)
            color: Qt.rgba(0.1, 1.0, 0.1, 1.0)
            ambientColor: Qt.rgba(0.2, 0.2, 0.2, 1.0)
        }

        Model {
            id: workshopModel
            visible: true
            scale: Qt.vector3d(100, 100, 100)
            rotation: Qt.quaternion(0, 0, 1, 0)
            geometry: WorkshopModel {
                objectName: "workshopModel"
            }
            materials: [
                PrincipledMaterial {
                    cullMode: PrincipledMaterial.NoCulling
                    baseColorMap: null
                    specularAmount: 0.5
                }
            ]
        }

        OrbitCameraController {
            origin: originNode
            camera: cameraNode
        }
    }
}
