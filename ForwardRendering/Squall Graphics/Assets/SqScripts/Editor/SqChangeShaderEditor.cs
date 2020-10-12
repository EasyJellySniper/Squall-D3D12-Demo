using UnityEngine;
using UnityEditor;
using UnityEngine.SceneManagement;
using UnityEditor.SceneManagement;

public class SqChangeShaderEditor : EditorWindow
{
    Shader oldShader;
    Shader newShader;

    /// <summary>
    /// init
    /// </summary>
    [MenuItem("Window/Sq Graphic/Change Shader")]
    static void Init()
    {
        GetWindow<SqChangeShaderEditor>();
    }


    void OnGUI()
    {
        oldShader = EditorGUILayout.ObjectField("Old shader:", oldShader, typeof(Shader), true) as Shader;
        newShader = EditorGUILayout.ObjectField("New shader:", newShader, typeof(Shader), true) as Shader;

        if (oldShader && newShader)
        {
            if (GUILayout.Button("Change Scene Shader"))
            {
                // change all shaders in this scene
                ChangeAllShadersInScene();
            }
        }
    }

    void ChangeAllShadersInScene()
    {
        GameObject[] sceneObjs = SceneManager.GetActiveScene().GetRootGameObjects();

        for (int i = 0; i < sceneObjs.Length; i++)
        {
            MeshRenderer[] meshRenderers = sceneObjs[i].GetComponentsInChildren<MeshRenderer>();

            for (int j = 0; j < meshRenderers.Length; j++)
            {
                Material[] mats = meshRenderers[j].sharedMaterials;

                for (int k = 0; k < mats.Length; k++)
                {
                    if (mats[k].shader == oldShader)
                        mats[k].shader = newShader;
                }
            }
        }

        EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
    }
}
