import QtQuick

Item
{
    id: root

    property string foreignText: ""
    property string nativeText: ""
    property real availableHeight: 0
    property real reservedHeight: 0

    readonly property int foreignSize:
    {
        for (let index = 0; index < candidates.count; ++index)
        {
            const candidate = candidates.itemAt(index)

            if (candidate && candidate.requiredHeight <= root.availableHeight)
            {
                return candidate.pixelSize
            }
        }

        return AppMetrics.sentenceMinimumSize
    }
    readonly property int nativeSize: Math.max(AppMetrics.sentenceNativeMinimumSize,
        AppMetrics.sentenceNativeSize - (AppMetrics.sessionTranslationSize - root.foreignSize))

    visible: false

    Repeater
    {
        id: candidates

        model: AppMetrics.sessionTranslationSize - AppMetrics.sentenceMinimumSize + 1

        delegate: Item
        {
            required property int index

            readonly property int pixelSize: AppMetrics.sessionTranslationSize - index
            readonly property real requiredHeight: (root.foreignText.length > 0 ? foreignMeasure.implicitHeight : 0)
                + (root.nativeText.length > 0 ? nativeMeasure.implicitHeight : 0)
                + (root.foreignText.length > 0 && root.nativeText.length > 0 ? AppMetrics.gapSmall : 0)
                + root.reservedHeight

            Text
            {
                id: foreignMeasure

                width: root.width
                text: root.foreignText
                textFormat: Text.PlainText
                font.family: AppMetrics.fontFamily
                font.pixelSize: parent.pixelSize
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
            }

            Text
            {
                id: nativeMeasure

                width: root.width
                text: root.nativeText
                textFormat: Text.PlainText
                font.family: AppMetrics.fontFamily
                font.pixelSize: Math.max(AppMetrics.sentenceNativeMinimumSize,
                    AppMetrics.sentenceNativeSize - (AppMetrics.sessionTranslationSize - parent.pixelSize))
                wrapMode: Text.WordWrap
            }
        }
    }
}
