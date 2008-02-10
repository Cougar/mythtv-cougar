<?php
/**
 * Print the program list
 *
 * @url         $URL$
 * @date        $Date$
 * @version     $Revision$
 * @author      $Author$
 * @license     GPL
 *
 * @package     MythWeb
 * @subpackage  TV
 *
/**/

// Set the desired page title
    $page_title = 'MythWeb - ' . t('Program Listing') . ': '.strftime($_SESSION['date_statusbar'], $list_starttime);

// Custom headers
    $headers[] = '<link rel="stylesheet" type="text/css" href="'.skin_url.'/tv_list.css">';

// Print the page header
    require 'modules/_shared/tmpl/'.tmpl.'/header.php';
?>

<script type="text/javascript">
    function list_update(timestamp) {
        ajax_add_request();
        new Ajax.Updater($('list_content'),
                         '<?php echo root ?>tv/list',
                         {
                            parameters: {
                                            ajax: true,
                                            time: timestamp
                                        },
                            onComplete: ajax_remove_request
                         }
                        );
    // We need to prevent hemorrhaging memory...
    // We don't use Tips.removeAll() as we are destroying the fields they are hooked into, so we don't need to mess with the DOM...
        Tips.tips.clear();
    }

    function load_tool_tip(element_id, channel_id, start_time) {
        var element = $(element_id);
        if (Tips.hasTip(element) == false) {
            ajax_add_request();
            new Ajax.Request('<?php echo root; ?>tv/get_show_details',
                             {
                                parameters: {
                                                chanid:             channel_id,
                                                starttime:          start_time,
                                                ajax:               true,
                                                backend_connect:    false
                                            },
                                onSuccess: add_tool_tip,
                                method:    'get'
                             });
        }
    }

    function add_tool_tip(content) {
        ajax_remove_request();
        var info = content.responseJSON;
        if (Tips.hasTip($(info['id'])) == false) {
            new Tip(info['id'], info['info'], { className: 'popup' });
            attempt_to_show_tip(info['id']);
            //setTimeout("attempt_to_show_tip('"+info['id']+"');", 5000);
        }
    }

    var currently_hovered_id = null;

    function attempt_to_show_tip(element) {
        if (element == currently_hovered_id)
            Tips.showTip(element);
    }
</script>

<div id="list_content">
    <?php require_once tmpl_dir.'list_data.php'; ?>
</div>
<?php
// Print the page footer
    require 'modules/_shared/tmpl/'.tmpl.'/footer.php';
