using System.Runtime.InteropServices;
using UnityEngine;

// sq sky box
public class SqSkybox : MonoBehaviour
{
    /// <summary>
    /// ambient ground
    /// </summary>
    [Header("Hemisphere Ambient Settings")]
    public Color ambientGround = Color.gray / 5;

    /// <summary>
    /// ambient sky
    /// </summary>
    public Color ambientSky = new Color(135 / 255f, 206 / 255f, 235 / 255f);

    [DllImport("SquallGraphics")]
    static extern void SetAmbientLight(Vector4 _ag, Vector4 _as);

    void Start()
    {
        SetAmbientLight(ambientGround, ambientSky);
    }

#if UNITY_EDITOR
    void OnValidate()
    {
        SetAmbientLight(ambientGround, ambientSky);
    }
#endif
}
