using UnityEngine;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine.SceneManagement;

/// <summary>
/// sq add renderer editor
/// </summary>
public class SqAddRendererEditor : EditorWindow
{
    Transform root;

    /// <summary>
    /// init
    /// </summary>
    [MenuItem("Window/Sq Graphic/Add Renderer")]
    static void Init()
    {
        GetWindow<SqAddRendererEditor>();
    }

    /// <summary>
    /// gui
    /// </summary>
    void OnGUI()
    {
        root = EditorGUILayout.ObjectField("Root: ", root, typeof(Transform), true) as Transform;
        if (!root)
        {
            return;
        }

        if (GUILayout.Button("Add Renderer"))
        {
            AddSqRenderer();
        }
    }

    /// <summary>
    /// add sq renderer
    /// </summary>
    void AddSqRenderer()
    {
        MeshRenderer[] renderers = root.GetComponentsInChildren<MeshRenderer>();
        
        for (int i = 0; i < renderers.Length; i++)
        {
            if (renderers[i].GetComponent<SqMeshRenderer>() != null)
            {
                continue;
            }

            renderers[i].gameObject.AddComponent<SqMeshFilter>();
            renderers[i].gameObject.AddComponent<SqMeshRenderer>();
        }

        EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
        EditorUtility.DisplayDialog("Sq Add Renderer", "Finish", "OK");
    }
}
