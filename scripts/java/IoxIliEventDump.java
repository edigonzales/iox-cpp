import ch.interlis.ili2c.Ili2c;
import ch.interlis.ili2c.config.Configuration;
import ch.interlis.ili2c.config.FileEntry;
import ch.interlis.ili2c.config.FileEntryKind;
import ch.interlis.ili2c.metamodel.TransferDescription;
import ch.interlis.iom.IomObject;
import ch.interlis.iom_j.xtf.Xtf24Reader;
import ch.interlis.iox.IoxEvent;
import ch.interlis.iox.IoxReader;
import ch.interlis.iox_j.IoxIliReader;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collections;
import java.util.List;

/** Emits the stable semantic subset shared by iox-ili and iox-cpp. */
public final class IoxIliEventDump {
    private static String atom(String value) {
        if (value == null) return "~";
        return Base64.getUrlEncoder().withoutPadding().encodeToString(
            value.getBytes(StandardCharsets.UTF_8));
    }

    private static String object(IomObject value) {
        StringBuilder result = new StringBuilder();
        String tag = value.getobjecttag();
        if (tag != null && tag.lastIndexOf('.') >= 0) {
            tag = tag.substring(tag.lastIndexOf('.') + 1);
        }
        result.append(atom(tag)).append('|');
        result.append(atom(value.getobjectoid())).append('|');
        result.append(value.getobjectoperation()).append('|');
        result.append(value.getobjectconsistency()).append('|');
        result.append(atom(value.getobjectrefoid())).append('|');
        result.append(atom(value.getobjectrefbid())).append('|');
        result.append(value.getobjectreforderpos()).append("|[");
        List<String> attributes = new ArrayList<String>();
        for (int attributeIndex = 0;
             attributeIndex < value.getattrcount(); ++attributeIndex) {
            String name = value.getattrname(attributeIndex);
            StringBuilder attribute = new StringBuilder();
            attribute.append(atom(name)).append('=');
            for (int valueIndex = 0;
                 valueIndex < value.getattrvaluecount(name); ++valueIndex) {
                if (valueIndex != 0) attribute.append(',');
                String primitive = value.getattrprim(name, valueIndex);
                if (primitive != null) {
                    attribute.append('p').append(atom(primitive));
                } else {
                    attribute.append("o{").append(
                        object(value.getattrobj(name, valueIndex))).append('}');
                }
            }
            attributes.add(attribute.toString());
        }
        Collections.sort(attributes);
        for (int index = 0; index < attributes.size(); ++index) {
            if (index != 0) result.append(';');
            result.append(attributes.get(index));
        }
        return result.append(']').toString();
    }

    private static TransferDescription compileModel(String path)
            throws ch.interlis.ili2c.Ili2cFailure {
        if ("-".equals(path)) return null;
        Configuration configuration = new Configuration();
        configuration.addFileEntry(
            new FileEntry(path, FileEntryKind.ILIMODELFILE));
        return Ili2c.runCompiler(configuration);
    }

    private static void emit(IoxEvent event) {
        if (event instanceof ch.interlis.iox.StartTransferEvent) {
            System.out.println("startTransfer");
        } else if (event instanceof ch.interlis.iox.StartBasketEvent) {
            ch.interlis.iox.StartBasketEvent value =
                (ch.interlis.iox.StartBasketEvent)event;
            System.out.println("startBasket\t" + atom(value.getType()) +
                "\t" + atom(value.getBid()) + "\t" + value.getKind() +
                "\t" + value.getConsistency());
        } else if (event instanceof ch.interlis.iox.ObjectEvent) {
            System.out.println("object\t" + object(
                ((ch.interlis.iox.ObjectEvent)event).getIomObject()));
        } else if (event instanceof ch.interlis.iox.EndBasketEvent) {
            System.out.println("endBasket");
        } else if (event instanceof ch.interlis.iox.EndTransferEvent) {
            System.out.println("endTransfer");
        } else {
            throw new IllegalStateException(
                "unsupported iox-ili event: " + event.getClass().getName());
        }
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2) {
            throw new IllegalArgumentException(
                "usage: IoxIliEventDump transfer.xtf model.ili|-");
        }
        IoxReader reader = Xtf24Reader.createReader(new File(arguments[0]));
        TransferDescription model = compileModel(arguments[1]);
        if (model != null) ((IoxIliReader)reader).setModel(model);
        try {
            while (true) {
                IoxEvent event = reader.read();
                if (event == null) break;
                emit(event);
                if (event instanceof ch.interlis.iox.EndTransferEvent) break;
            }
        } finally {
            reader.close();
        }
    }
}
